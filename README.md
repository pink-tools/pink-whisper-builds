# pink-whisper

TCP server for whisper.cpp speech-to-text. Model loads once and stays in memory.

## Install

Download binary from [Releases](https://github.com/pink-tools/pink-whisper/releases) and model [ggml-large-v3.bin](https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3.bin) (~3GB).

## Usage

```bash
./pink-whisper -m ggml-large-v3.bin -p 7465
```

Options:
- `-m PATH` - model file (default: ggml-large-v3.bin)
- `-p PORT` - listen port (default: 7465)

## Protocol

Binary protocol over TCP:

```
Request:  [4 bytes LE size][audio data]
Response: [4 bytes LE size][UTF-8 text]
```

Audio format: 16-bit PCM, 16kHz, mono.

```python
import socket, struct
sock = socket.create_connection(("127.0.0.1", 7465))
sock.send(struct.pack("<I", len(pcm)) + pcm)
size = struct.unpack("<I", sock.recv(4))[0]
text = sock.recv(size).decode()
```
