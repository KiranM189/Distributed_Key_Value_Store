import requests
import hashlib

# Define available nodes
NODES = ["http://localhost:5000", "http://localhost:5001"]

# Hashing function to determine the target node
def hash_key(key):
    return int(hashlib.sha256(key.encode()).hexdigest(), 16) % len(NODES)

# Function to dynamically put key-value pairs
def put(key, value):
    node = NODES[hash_key(key)]
    response = requests.post(f"{node}/put", json={"key": key, "value": value})
    print(response.json())

# Function to dynamically get a value by key
def get(key):
    node = NODES[hash_key(key)]
    response = requests.get(f"{node}/get/{key}")
    print(response.json())

# Function to dynamically delete a key
def delete(key):
    node = NODES[hash_key(key)]
    response = requests.delete(f"{node}/delete/{key}")
    print(response.json())

# Main menu loop
def main():
    while True:
        print("\n--- Key-Value Store Menu ---")
        print("1. Add a Key-Value Pair")
        print("2. Retrieve a Value by Key")
        print("3. Delete a Key")
        print("4. Exit")
       
        choice = input("Enter your choice (1/2/3/4): ")
       
        if choice == "1":
            key = input("Enter the key: ")
            value = input("Enter the value: ")
            put(key, value)
       
        elif choice == "2":
            key = input("Enter the key to retrieve: ")
            get(key)
       
        elif choice == "3":
            key = input("Enter the key to delete: ")
            delete(key)
       
        elif choice == "4":
            print("Exiting... Goodbye!")
            break
       
        else:
            print("Invalid choice. Please try again.")

if __name__ == "__main__":
    main()
