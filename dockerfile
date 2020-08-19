FROM python:3.8.5

# Install build dependencies
RUN pip3 install meson
RUN apt-get update && apt-get install -y ninja-build mingw-w64

# Copy source files
COPY . .

# Build 32-bit Sega Tools
RUN meson --cross cross-mingw-32.txt _build32
RUN ninja -C _build32

# Build 64-bit Sega Tools
RUN meson --cross cross-mingw-64.txt _build64
RUN ninja -C _build64

# Clean and prepare post-build script files
RUN sed -i 's/\r$//' mkdist && chmod +x mkdist

# Run post-build script files
# (bash needed for brace expansion)
RUN bash ./mkdist
