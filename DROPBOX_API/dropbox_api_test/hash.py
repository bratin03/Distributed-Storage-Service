import hashlib
import sys

def compute_dropbox_content_hash(file_path):
    block_size = 4 * 1024 * 1024  # 4MB blocks
    sha256_hashes = []

    try:
        with open(file_path, "rb") as f:
            while True:
                block = f.read(block_size)
                if not block:
                    break
                # Compute SHA-256 for the current block
                sha256_hashes.append(hashlib.sha256(block).digest())
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)

    # Concatenate all block hashes and compute SHA-256 of the result
    overall_hash = hashlib.sha256(b"".join(sha256_hashes)).hexdigest()
    return overall_hash

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python compute_hash.py <filename>")
        sys.exit(1)
    
    file_name = sys.argv[1]
    content_hash = compute_dropbox_content_hash(file_name)
    print(f"Dropbox content hash for '{file_name}':")
    print(content_hash)
