# pink-whisper

TCP server for whisper.cpp speech-to-text. Loads model once, serves transcription requests over binary TCP protocol.

**Repository:** https://github.com/pink-tools/pink-whisper

**Language:** C++

## File Structure

```
pink-whisper/
├── src/
│   └── pink-whisper.cpp      # Complete application (~180 lines)
├── .github/
│   └── workflows/
│       └── build.yml         # Multi-platform CI/CD
├── ai-docs/
│   └── CLAUDE.md             # This file
├── README.md
├── RELEASE_NOTES.md
└── .gitignore
```

## Architecture

Single-file C++ application with three components:

1. **TCP Server** - Listens on port 7465, single-threaded accept loop
2. **Audio Processor** - Converts 16-bit PCM to float32 for Whisper
3. **Whisper Inference** - whisper.cpp with greedy sampling, auto language detection

```
Client connects
    ↓
Receive: [4 bytes LE size][audio data]
    ↓
Convert: int16 PCM → float32
    ↓
Whisper inference (up to 4 threads)
    ↓
Send: [4 bytes LE size][UTF-8 text]
    ↓
Close connection
```

### Global State

```cpp
struct whisper_context* g_ctx = nullptr;  // Model loaded once at startup
```

### Whisper Configuration

```cpp
whisper_context_params cparams = whisper_context_default_params();
cparams.use_gpu = true;
cparams.flash_attn = true;

whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
wparams.language = "auto";
wparams.n_threads = min(4, hardware_concurrency);
```

## TCP Protocol

**Port:** 7465 (TCP, one request per connection)

### Request

```
┌──────────────────┬─────────────────────────┐
│ 4 bytes (LE)     │ N bytes                 │
│ uint32: size     │ 16-bit PCM, 16kHz, mono │
└──────────────────┴─────────────────────────┘
```

### Response

```
┌──────────────────┬─────────────────────────┐
│ 4 bytes (LE)     │ M bytes                 │
│ uint32: size     │ UTF-8 text              │
└──────────────────┴─────────────────────────┘
```

### Audio Requirements

| Property | Value |
|----------|-------|
| Format | Raw PCM (no header) |
| Sample rate | 16000 Hz |
| Channels | 1 (mono) |
| Bit depth | 16-bit signed |
| Byte order | Little-endian |

### Python Client Example

```python
import socket, struct

def transcribe(pcm: bytes, host="127.0.0.1", port=7465) -> str:
    sock = socket.create_connection((host, port))
    sock.send(struct.pack("<I", len(pcm)) + pcm)
    size = struct.unpack("<I", sock.recv(4))[0]
    return sock.recv(size).decode()
```

## Build Process

Project doesn't build standalone. CI workflow:
1. Checks out `ggml-org/whisper.cpp`
2. Downloads `pink-whisper.cpp` from this repo
3. Injects into whisper.cpp CMakeLists.txt
4. Builds as whisper.cpp example

### Build Variants

| Artifact | Platform | Acceleration |
|----------|----------|--------------|
| darwin-arm64-coreml.tar.gz | macOS ARM64 | CoreML (Apple Neural Engine) |
| linux-amd64-cpu.tar.gz | Linux x64 | CPU |
| linux-amd64-cuda.tar.gz | Linux x64 | CUDA 12 |
| windows-amd64-cpu.zip | Windows x64 | CPU |
| windows-amd64-cuda.zip | Windows x64 | CUDA 12 |

### CMake Injection

```cmake
add_executable(pink-whisper examples/pink-whisper/pink-whisper.cpp)
target_link_libraries(pink-whisper PRIVATE whisper)  # + ws2_32 on Windows
```

### Platform-Specific Builds

**macOS CoreML:**
```bash
./models/generate-coreml-model.sh large-v3  # Generates mlmodelc
cmake -B build -DWHISPER_COREML=1 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
```

**Linux CUDA:** Container `nvidia/cuda:12.6.2-devel-ubuntu24.04`, bundles libcudart.so.12, libcublas.so.12, libcublasLt.so.12

**Windows CUDA:** Downloads CUDA 12.4 toolkit components, bundles cudart64_12.dll, cublas64_12.dll, cublasLt64_12.dll

## Dependencies

### Runtime

| Variant | Required Files |
|---------|----------------|
| All | ggml-large-v3.bin (~3GB) |
| macOS CoreML | ggml-large-v3-encoder.mlmodelc/ |
| CUDA Linux | libcudart.so.12, libcublas.so.12, libcublasLt.so.12 |
| CUDA Windows | cudart64_12.dll, cublas64_12.dll, cublasLt64_12.dll |

### Model

```bash
curl -L -o ggml-large-v3.bin \
  https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin
```

## CLI

```bash
./pink-whisper [-m MODEL] [-p PORT]
```

| Flag | Default | Description |
|------|---------|-------------|
| -m | ggml-large-v3.bin | Model file path |
| -p | 7465 | TCP listen port |

**Startup output:**
```
pink-whisper: loading model ggml-large-v3.bin
pink-whisper: model loaded
pink-whisper: listening on port 7465
```

## Performance

- Model loading: 10-30 seconds
- Memory: ~3-4GB RAM, ~2-3GB VRAM (GPU)
- Inference threads: up to 4
- Connection handling: single-threaded (one request at a time)

## Error Handling

- Model load failure: exit 1
- Socket errors: exit 1
- Accept failure: continue loop
- Recv/send failure: close connection, continue
- Inference failure: returns `[error: inference failed]`

## Related Projects

- **pink-transcriber** - Go CLI wrapper, auto-downloads pink-whisper + model
- **pink-voice** - Voice input daemon using pink-transcriber
- **whisper.cpp** - Upstream library (github.com/ggml-org/whisper.cpp)
