# EXP-2026-11-19-1304: the baseline rebuilds byte-identically, and one experiment did not survive

## The reproduction check

Rebuilding from the committed revision and comparing with the registered checkpoint:

```
builds/flint-acc/bin/libllama.so.0.7.0             d5c566d0b9db7e5804ca36e02e7242994e79d92f671373c423ef677b9b53194f
builds/checkpoint-20261119-EXP1298/libllama.so.0.7.0  d5c566d0b9db7e5804ca36e02e7242994e79d92f671373c423ef677b9b53194f
```

Identical. The registered baseline is reproducible from source, not only restorable from its binaries - which is a
stronger statement than the restore test in the checkpoint manifest, and the two together cover both directions.

## The experiment that was reverted

The last remaining sampling difference (adaptive-p's transform path, 21 of 100 rounds) was attacked from its most
plausible direction: the door normalised the probabilities **and** the host normalised them only on its own path, so
the two used different maxima. The change moved the normalisation out of the door and into a single host call for
both paths.

It did not work, and it left the tree in a state I could not explain: the goose path printed **untransformed**
logits while its own flag said the door had run, and the metric stayed at 21. Rather than keep a change whose
behaviour contradicts its intent, both files were restored to the committed revision and the build re-verified:

```
after restore: transform path 21 of 100 differing   (the known state)
accepted components: all present
library hash equals the EXP1298 checkpoint
```

So the requirement this thread has been circling stays open, and it stays stated: adaptive-p's transform path
differs from the native in a way that survives identical arrays, identical probabilities and identical logits at the
divergence. It is off the acceptance path, the baseline is unaffected, and the next attempt starts from this
recorded state rather than from an unexplained edit.
