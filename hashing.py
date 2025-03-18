import hashlib

def hash_key(key, num_nodes):
    hash_value = int(hashlib.sha256(key.encode()).hexdigest(), 16)
    return hash_value % num_nodes

# Example
num_nodes = 4
key = "user123"
node = hash_key(key, num_nodes)
print(f"Key '{key}' is stored in node {node}")
