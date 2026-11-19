#!/usr/bin/env python3
"""Numeric oracle for the Maxwell trunk, transcribed from modeling_maxwell_moe.py.

Runs on F32 weights and compares against the runtime's logits for the same
synthetic GGUF. This checks the graph's semantics (mHC, Sinkhorn, QK-norm,
GQA+RoPE, sigmoid MoE, shared expert, learned HC head), not quantisation.
"""
import json
import os
import subprocess
import sys
import tempfile

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "experiments", "EXP-2026-11-17-927-maxwell-quant-convert", "tests"))

import importlib.util
spec = importlib.util.spec_from_file_location("conv", os.path.join(ROOT, "tools", "maxwell-convert.py"))
conv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(conv)

import test_convert as T  # synthetic config and tensors


def sigmoid(x):
    return 1.0/(1.0+np.exp(-x))


def rmsnorm(x, eps, weight=None):
    v = np.mean(x.astype(np.float32)**2, axis=-1, keepdims=True)
    y = x/np.sqrt(v+eps)
    return y*weight if weight is not None else y


def rope(x, pos, theta, dim):
    inv = 1.0/(theta ** (np.arange(0, dim, 2, dtype=np.float64)/dim))
    freqs = np.outer(pos, inv)                       # [s, dim/2]
    emb = np.concatenate([freqs, freqs], -1)
    cos = np.cos(emb)[:, None, :]
    sin = np.sin(emb)[:, None, :]
    x1 = x[..., :dim//2]
    x2 = x[..., dim//2:]
    rot = np.concatenate([-x2, x1], -1)
    return (x*cos + rot*sin).astype(np.float32)


class Oracle:
    def __init__(self, cfg, w):
        self.cfg = cfg
        self.w = w
        self.C = cfg["hidden_size"]
        self.H = cfg["num_attention_heads"]
        self.Hkv = cfg["num_key_value_heads"]
        self.hd = cfg["attention_head_size"]
        self.eps = cfg["rms_norm_eps"]
        self.hc = cfg["num_residual_streams"]
        self.iters = cfg["mhc_sinkhorn_iterations"]
        self.hceps = 1e-6
        self.theta = cfg["rope_theta"]
        self.dense = set(cfg["mlp_only_layers"])

    def g(self, name):
        return self.w[name].astype(np.float32)

    def mlp(self, x, p):
        w1, w3, w2 = self.g(p+".w1.weight"), self.g(p+".w3.weight"), self.g(p+".w2.weight")
        h = x @ w1.T
        h = h*sigmoid(h)          # silu
        return (h * (x @ w3.T)) @ w2.T

    def hc_gates(self, x, tag):
        W = self.g(tag+".mapping_proj.weight")
        bias = self.g(tag+".bias")
        ap = float(self.g(tag+".alpha_pre").reshape(-1)[0])
        ao = float(self.g(tag+".alpha_post").reshape(-1)[0])
        ar = float(self.g(tag+".alpha_res").reshape(-1)[0])
        alpha = np.concatenate([np.full(self.hc, ap), np.full(self.hc, ao),
                                np.full(self.hc*self.hc, ar)]).astype(np.float32)
        flat = x.reshape(x.shape[0], -1)
        nflat = flat/np.sqrt(np.mean(flat**2, -1, keepdims=True)+self.eps)
        h = (nflat @ W.T)*alpha + bias
        n = self.hc
        pre = sigmoid(h[:, :n]) + self.hceps
        post = sigmoid(h[:, n:2*n])*2.0
        logits = h[:, 2*n:].reshape(-1, n, n)
        comb = np.exp(logits - logits.max(-1, keepdims=True))
        comb = comb/comb.sum(-1, keepdims=True) + self.hceps
        comb = comb/(comb.sum(-2, keepdims=True)+self.hceps)
        for _ in range(self.iters-1):
            comb = comb/(comb.sum(-1, keepdims=True)+self.hceps)
            comb = comb/(comb.sum(-2, keepdims=True)+self.hceps)
        agg = (x.reshape(-1, n, self.C)*pre[:, :, None]).sum(1)
        return agg, post, comb

    def hc_post(self, out, res, post, comb):
        n = self.hc
        rs = res.reshape(-1, n, self.C)
        mixed = np.einsum("sim,sic->smc", comb, rs)
        return ((post[:, :, None]*out[:, None, :]) + mixed).reshape(res.shape[0], -1)

    def attn(self, x, p, pos):
        C, H, Hkv, hd = self.C, self.H, self.Hkv, self.hd
        s = x.shape[0]
        q = (x @ self.g(p+".q_proj.weight").T).reshape(s, H, hd)
        k = (x @ self.g(p+".k_proj.weight").T).reshape(s, Hkv, hd)
        v = (x @ self.g(p+".v_proj.weight").T).reshape(s, Hkv, hd)
        q = rmsnorm(q, self.eps, self.g(p+".q_layernorm.weight")).astype(np.float32)
        k = rmsnorm(k, self.eps, self.g(p+".k_layernorm.weight")).astype(np.float32)
        q = rope(q, pos, self.theta, hd)
        k = rope(k, pos, self.theta, hd)
        rep = H//Hkv
        k = np.repeat(k, rep, axis=1)
        v = np.repeat(v, rep, axis=1)
        scores = np.einsum("shd,thd->hst", q, k)/np.sqrt(hd)
        mask = np.triu(np.ones((s, s), dtype=bool), 1)
        scores = np.where(mask[None], -np.inf, scores)
        probs = np.exp(scores - scores.max(-1, keepdims=True))
        probs = probs/probs.sum(-1, keepdims=True)
        ctx = np.einsum("hst,thd->shd", probs, v).reshape(s, H*hd)
        return ctx @ self.g(p+".o_proj.weight").T

    def moe(self, x, p):
        gate = self.g(p+".gate.weight")
        bias = self.g(p+".expert_bias")
        s = sigmoid(x @ gate.T)                       # [s, E]
        sel = np.argsort(-(s + bias), axis=-1)[:, : self.cfg["num_experts_per_tok"]]
        w = np.take_along_axis(s, sel, axis=1)
        w = w/w.sum(-1, keepdims=True)
        out = np.zeros_like(x)
        # the synthetic model is small enough for an explicit token/slot loop
        for t in range(x.shape[0]):
            for j, e in enumerate(sel[t]):
                out[t] += w[t, j]*self.mlp(x[t], f"{p}.experts.{e}")
        out += self.mlp(x, p+".shared_expert")
        return out

    def forward(self, tokens):
        w = self.w
        pos = np.arange(len(tokens), dtype=np.float64)
        x = w["model.embed_tokens.weight"].astype(np.float32)[tokens]   # [s, C]
        x = np.repeat(x[:, None, :], self.hc, axis=1).reshape(len(tokens), -1)
        for i in range(self.cfg["num_hidden_layers"]):
            p = f"model.layers.{i}"
            res = x
            agg, post, comb = self.hc_gates(x, p+".self_attn_hyper_connection")
            agg = rmsnorm(agg, self.eps, self.g(p+".input_layernorm.weight"))
            out = self.attn(agg, p+".self_attn", pos)
            x = self.hc_post(out, res, post, comb)

            res = x
            agg, post, comb = self.hc_gates(x, p+".mlp_hyper_connection")
            agg = rmsnorm(agg, self.eps, self.g(p+".post_attention_layernorm.weight"))
            if i in self.dense:
                out = self.mlp(agg, p+".block_sparse_moe")
            else:
                out = self.moe(agg, p+".block_sparse_moe")
            x = self.hc_post(out, res, post, comb)

        fn = self.g("model.hc_head.hc_head_fn")
        base = self.g("model.hc_head.hc_head_base")
        scale = self.g("model.hc_head.hc_head_scale")
        flat = x
        rsq = 1.0/np.sqrt(np.mean(flat**2, -1, keepdims=True)+self.hceps)
        wgt = sigmoid((flat @ fn.T)*rsq*scale + base) + self.hceps
        h = (x.reshape(-1, self.hc, self.C)*wgt[:, :, None]).sum(1)
        h = rmsnorm(h, self.eps, self.g("model.norm.weight"))
        return h @ self.g("lm_head.weight").T


def main():
    rng = np.random.default_rng(11)
    tensors = T.build_tensors(rng)
    by = dict(tensors)
    td = os.environ.get("MX_SYNTH_DIR", "/home/end0/AI/LlamaServer/maxwell-build-20261117/synth")

    oracle = Oracle(T.CFG, by)
    tokens = [1, 2, 3, 4]
    logits = oracle.forward(tokens)[-1]

    rt = np.loadtxt(os.path.join(td, "runtime.logits"), dtype=np.float64)
    if rt.shape != logits.shape:
        print(f"shape mismatch runtime={rt.shape} oracle={logits.shape}")
        return 1
    diff = np.abs(rt - logits)
    denom = np.abs(logits) + 1e-6
    print(f"n={logits.size} max_abs={diff.max():.6g} mean_abs={diff.mean():.6g} "
          f"max_rel={(diff/denom).max():.4g} argmax rt={int(rt.argmax())} oracle={int(logits.argmax())}")
    ok = diff.max() < 5e-3 and int(rt.argmax()) == int(logits.argmax())
    print("MAXWELL-NUMERIC OK" if ok else "MAXWELL-NUMERIC MISMATCH")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
