#include "deterministic_workload.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace bmoe {

std::unique_ptr<DeterministicWorkload> DeterministicWorkload::capture() {
    return std::unique_ptr<DeterministicWorkload>(new DeterministicWorkload(Mode::Capture));
}

std::unique_ptr<DeterministicWorkload> DeterministicWorkload::load(const std::string & path, std::string & error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open deterministic workload '" + path + "'";
        return nullptr;
    }
    std::string magic;
    int version = 0;
    if (!(in >> magic >> version) || magic != "BMOE_DETERMINISTIC_WORKLOAD" || version != 1) {
        error = "unsupported deterministic workload header in '" + path + "'";
        return nullptr;
    }

    auto out = std::unique_ptr<DeterministicWorkload>(new DeterministicWorkload(Mode::Replay));
    std::string key;
    if (!(in >> key >> out->arch_) || key != "arch" || !(in >> key >> out->n_layer_) || key != "layers" ||
        !(in >> key >> out->n_expert_used_) || key != "top_k") {
        error = "invalid deterministic workload metadata in '" + path + "'";
        return nullptr;
    }

    size_t n = 0;
    if (!(in >> key >> n) || key != "prompt") {
        error = "missing prompt tokens in deterministic workload '" + path + "'";
        return nullptr;
    }
    out->prompt_tokens_.resize(n);
    for (int32_t & token : out->prompt_tokens_)
        if (!(in >> token)) {
            error = "truncated prompt tokens in deterministic workload '" + path + "'";
            return nullptr;
        }

    if (!(in >> key >> n) || key != "output") {
        error = "missing output tokens in deterministic workload '" + path + "'";
        return nullptr;
    }
    out->output_tokens_.resize(n);
    for (int32_t & token : out->output_tokens_)
        if (!(in >> token)) {
            error = "truncated output tokens in deterministic workload '" + path + "'";
            return nullptr;
        }

    if (!(in >> key >> n) || key != "routes") {
        error = "missing routes in deterministic workload '" + path + "'";
        return nullptr;
    }
    out->routes_.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        Route r;
        size_t n_ids = 0;
        if (!(in >> key >> r.phase >> r.base_pos >> r.batch_n >> r.layer >> r.top_k >> r.token_n >> n_ids) ||
            key != "route" || n_ids != (size_t) r.top_k * (size_t) r.token_n) {
            error = "invalid route record " + std::to_string(i) + " in deterministic workload '" + path + "'";
            return nullptr;
        }
        r.ids.resize(n_ids);
        for (int32_t & id : r.ids)
            if (!(in >> id)) {
                error = "truncated route record " + std::to_string(i) + " in deterministic workload '" + path + "'";
                return nullptr;
            }
        out->routes_.push_back(std::move(r));
    }
    return out;
}

bool DeterministicWorkload::fail(std::string message) {
    if (error_.empty()) error_ = std::move(message);
    return false;
}

void DeterministicWorkload::set_static(std::string arch, int n_layer, int n_expert_used) {
    arch_ = std::move(arch);
    n_layer_ = n_layer;
    n_expert_used_ = n_expert_used;
}

bool DeterministicWorkload::check_static(const std::string & arch, int n_layer, int n_expert_used) {
    if (arch_ != arch || n_layer_ != n_layer || n_expert_used_ != n_expert_used) {
        std::ostringstream msg;
        msg << "workload/model mismatch: captured arch=" << arch_ << " layers=" << n_layer_ << " top_k="
            << n_expert_used_ << ", current arch=" << arch << " layers=" << n_layer << " top_k=" << n_expert_used;
        return fail(msg.str());
    }
    return true;
}

bool DeterministicWorkload::prompt(std::vector<int32_t> & tokens) {
    if (mode_ == Mode::Capture) {
        prompt_tokens_ = tokens;
        return true;
    }
    if (tokens != prompt_tokens_) return fail("tokenized prompt differs from the captured workload");
    return true;
}

void DeterministicWorkload::begin_batch(int base_pos, int n_tokens, int phase) {
    batch_base_pos_ = base_pos;
    batch_n_ = n_tokens;
    batch_phase_ = phase;
}

bool DeterministicWorkload::capture_topk(int layer, int n_expert_used, int n_tokens, const int32_t * ids) {
    if (!error_.empty()) return false;
    Route current;
    current.phase = batch_phase_;
    current.base_pos = batch_base_pos_;
    current.batch_n = batch_n_;
    current.layer = layer;
    current.top_k = n_expert_used;
    current.token_n = n_tokens;
    current.ids.assign(ids, ids + (size_t) n_expert_used * (size_t) n_tokens);
    routes_.push_back(std::move(current));
    return true;
}

bool DeterministicWorkload::replay_topk(int layer, int n_expert_used, int n_tokens, const int32_t *& ids) {
    if (!error_.empty()) return false;
    if (route_cursor_ >= routes_.size()) return fail("replay produced more route records than the workload contains");
    const Route & expected = routes_[route_cursor_++];
    if (expected.phase != batch_phase_ || expected.base_pos != batch_base_pos_ || expected.batch_n != batch_n_ ||
        expected.layer != layer || expected.top_k != n_expert_used || expected.token_n != n_tokens) {
        std::ostringstream msg;
        msg << "route shape mismatch at record " << (route_cursor_ - 1) << ": expected phase/base/batch/layer/top_k/tokens "
            << expected.phase << '/' << expected.base_pos << '/' << expected.batch_n << '/' << expected.layer << '/'
            << expected.top_k << '/' << expected.token_n << ", got " << batch_phase_ << '/' << batch_base_pos_ << '/'
            << batch_n_ << '/' << layer << '/' << n_expert_used << '/' << n_tokens;
        return fail(msg.str());
    }
    ids = expected.ids.data();
    return true;
}

bool DeterministicWorkload::token(size_t index, int32_t natural, int32_t & selected) {
    if (!error_.empty()) return false;
    if (mode_ == Mode::Capture) {
        if (index != output_tokens_.size()) return fail("captured output token index is not sequential");
        output_tokens_.push_back(natural);
        selected = natural;
        return true;
    }
    if (index >= output_tokens_.size()) return fail("replay requested more output tokens than the workload contains");
    selected = output_tokens_[index];
    return true;
}

bool DeterministicWorkload::finish(size_t emitted_tokens) {
    if (!error_.empty()) return false;
    if (mode_ == Mode::Replay) {
        if (route_cursor_ != routes_.size())
            return fail("replay consumed " + std::to_string(route_cursor_) + " of " + std::to_string(routes_.size()) +
                        " route records");
        if (emitted_tokens != output_tokens_.size())
            return fail("replay emitted " + std::to_string(emitted_tokens) + " of " +
                        std::to_string(output_tokens_.size()) + " captured tokens");
    }
    return true;
}

bool DeterministicWorkload::save(const std::string & path) {
    if (mode_ != Mode::Capture) return fail("only a captured workload can be saved");
    std::ofstream out(path, std::ios::trunc);
    if (!out) return fail("cannot write deterministic workload '" + path + "'");
    out << "BMOE_DETERMINISTIC_WORKLOAD 1\n";
    out << "arch " << arch_ << "\nlayers " << n_layer_ << "\ntop_k " << n_expert_used_ << '\n';
    out << "prompt " << prompt_tokens_.size();
    for (int32_t token : prompt_tokens_) out << ' ' << token;
    out << "\noutput " << output_tokens_.size();
    for (int32_t token : output_tokens_) out << ' ' << token;
    out << "\nroutes " << routes_.size() << '\n';
    for (const Route & r : routes_) {
        out << "route " << r.phase << ' ' << r.base_pos << ' ' << r.batch_n << ' ' << r.layer << ' ' << r.top_k << ' '
            << r.token_n << ' ' << r.ids.size();
        for (int32_t id : r.ids) out << ' ' << id;
        out << '\n';
    }
    if (!out) return fail("failed while writing deterministic workload '" + path + "'");
    return true;
}

} // namespace bmoe
