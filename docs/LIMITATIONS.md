# Alpha limitations

- The supported chat profile is Apex-35B-V1.0 Q4_K_M. See the
  [model registry](goose-migration/MODEL-ARCHITECTURES.md) for other architectures.
- The documented cumulative performance numbers are for one response at a time
  on the recorded RTX 4070 setup. Batch, context, GPU and expert-cache capacity
  affect performance and memory.
- The alpha API supports text messages and Apex function calling. Structured
  output, forced tool selection and multimodal messages are outside its scope.
- Windows release evidence covers Wine and the documented transport checks.
  Native NVIDIA inference requires its own qualification.
- The [Linux build](BUILD.md) uses pinned compiler and checkpoint inputs.
  The source tree alone does not contain every build artifact.
