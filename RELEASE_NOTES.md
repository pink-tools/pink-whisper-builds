- `darwin-arm64-coreml.tar.gz` - macOS ARM64 (CoreML)
- `linux-amd64-cpu.tar.gz` - Linux x64
- `linux-amd64-cuda.tar.gz` - Linux x64 (CUDA 12)
- `windows-amd64-cpu.zip` - Windows x64
- `windows-amd64-cuda.zip` - Windows x64 (CUDA 12)

Requires model: [ggml-large-v3.bin](https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin) (~3GB)

**Structure:**

```
pink-whisper
ggml-large-v3.bin
ggml-large-v3-encoder.mlmodelc/  # macOS only
libcublas*.so / cublas*.dll      # CUDA only
```
