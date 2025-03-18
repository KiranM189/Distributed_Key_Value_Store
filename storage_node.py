from flask import Flask, request, jsonify
import redis

app = Flask(__name__)
db = redis.Redis(host='localhost', port=6379, decode_responses=True)

@app.route('/put', methods=['POST'])
def put_value():
    data = request.json
    db.set(data['key'], data['value'])
    return jsonify({"message": "Stored successfully!"})

@app.route('/get/<key>', methods=['GET'])
def get_value(key):
    value = db.get(key)
    return jsonify({"value": value if value else "Not found"})

@app.route('/delete/<key>', methods=['DELETE'])
def delete_value(key):
    db.delete(key)
    return jsonify({"message": "Deleted successfully!"})

if __name__ == '__main__':
    app.run(port=5000)
