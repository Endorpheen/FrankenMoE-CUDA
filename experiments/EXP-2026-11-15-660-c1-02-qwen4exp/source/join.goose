// Join module for the C1-02 integration gate: it pulls the canonical qwen4exp
// plan/shape slice and the tensor map into one compilation unit, so the gate
// can prove both id spaces name the same tensors.
import qwen4exp_canon;
import q4t_tensor_map;
