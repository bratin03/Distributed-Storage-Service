from node import Node
from node import FOLLOWER, LEADER
from auth import verify_jwt
from flask import Flask, request, jsonify
import sys
import logging
import json
logging.getLogger("urllib3").setLevel(logging.WARNING)
logging.getLogger("requests").setLevel(logging.WARNING)
import configparser
import requests

# Load configuration from the config.ini file
config = configparser.ConfigParser()
config.read("config.ini")

app = Flask(__name__)

# value_get is the flask handle
@app.route("/request", methods=["GET"])
def value_get():
    payload = request.json["payload"]
    reply = {"code": "fail", "payload": payload}
    if n.status == LEADER:
        token = payload["token"]
        filePath = payload["key"]

        userID = verify_jwt(token)
        if userID is None:
            reply = {"code": "fail", "message": "Invalid token"}
            return jsonify(reply)
        
        key = userID + ":" + filePath
        result = n.handle_get(key)
        if result:
            reply = {"code": "success", "payload": result}
    elif n.status == FOLLOWER:
        # redirect request
        reply["payload"]["message"] = n.leader
    return jsonify(reply)


# Retrieve the IP address and port from the configuration file.
meta_server_ip = config.get("meta_server", "ip", fallback="127.0.0.1")
meta_server_port = config.get("meta_server", "port", fallback="5000")

def notification_handler(message):
    """
    Sends a notification to the block server confirmation route.

    Parameters:
        message (dict): A dictionary containing notification data.
    """
    # Construct the URL to hit the /block-server-confirmation route
    url = f"http://{meta_server_ip}:{meta_server_port}/block-server-confirmation"

    try:
        # Send a POST request with the message as JSON payload.
        response = requests.post(url, json=message)
        response.raise_for_status()  # Raises an HTTPError if the status is 4xx or 5xx.
        print(f"Notification sent successfully to {url}. Response: {response.text}")
    except requests.RequestException as e:
        print(f"Failed to send notification to {url}. Error: {e}")

@app.route("/request", methods=["PUT"])
def value_put():
    payload = request.json["payload"]
    reply = {"code": "fail"}

    if n.status == LEADER:
        payload["value"] = json.loads(payload["value"])

        token = payload["token"]
        filePath = payload["key"]

        userID = verify_jwt(token)
        if userID is None:
            reply = {"code": "fail", "message": "Invalid token"}
            return jsonify(reply)
        
        new_version = payload["value"]["version_number"]

        key = userID + ":" + filePath
        existing_payload = n.handle_get(key)

        # deletion of file
        if payload["value"] == "__DELETE__":
            new_payload = {"key": key, "value": "__DELETE__"}
            result =  n.handle_put(new_payload)
            if result:
                reply = {"code": "success", "payload": new_payload}
            else:
                reply = {"code": "fail", "message": "Deletion failed"}

        # creation of file
        elif existing_payload is None or existing_payload == "__DELETE__":
            if new_version != "0":
                reply = {"code": "fail", "message": "Version mismatch"}
            else : 
                new_payload = {"key": key, "value": json.dumps(new_payload["value"])}
                result = n.handle_put(new_payload)
                if result:
                    reply = {"code": "success", "payload": new_payload}
                    notification_handler({"user_id": userID, "path": filePath})
                else:
                    reply = {"code": "fail", "message": "Creation failed"}

        # update of file     
        else :
            existing_payload = json.loads(existing_payload)
            existing_version = int(existing_payload["version_number"])

            print("-->", existing_version, " -- ", new_version)
            if existing_version != new_version:
                reply = {"code": "fail", "message": "Version mismatch"}
                return jsonify(reply)
            else:
                new_version += 1
                new_payload["value"]["version_number"] = str(new_version)
                
                new_payload = {"key": key, "value": json.dumps(new_payload["value"])}
                result = n.handle_put(new_payload)
                if result:
                    reply = {"code": "success", "payload": new_payload}
                    notification_handler({"user_id": userID, "path": filePath})
                else:
                    reply = {"code": "fail", "message": "Creation failed"}

    elif n.status == FOLLOWER:
        # redirect request
        payload["message"] = n.leader
        reply["payload"] = payload
    return jsonify(reply)


# we reply to vote request
@app.route("/vote_req", methods=["POST"])
def vote_req():
    # also need to let me know whether up-to-date or not
    term = request.json["term"]
    commitIdx = request.json["commitIdx"]
    staged = request.json["staged"]
    choice, term = n.decide_vote(term, commitIdx, staged)
    message = {"choice": choice, "term": term}
    return jsonify(message)


@app.route("/heartbeat", methods=["POST"])
def heartbeat():
    term, commitIdx = n.heartbeat_follower(request.json)
    # return anyway, if nothing received by leader, we are dead
    message = {"term": term, "commitIdx": commitIdx}
    return jsonify(message)


# disable flask logging
log = logging.getLogger("werkzeug")
log.disabled = True

if __name__ == "__main__":
    # python server.py index ip_list
    if len(sys.argv) == 3:
        index = int(sys.argv[1])
        ip_list_file = sys.argv[2]
        ip_list = []
        # open ip list file and parse all the ips
        with open(ip_list_file) as f:
            for ip in f:
                ip_list.append(ip.strip())
        my_ip = ip_list.pop(index)

        http, host, port = my_ip.split(":")
        # initialize node with ip list and its own ip
        n = Node(ip_list, my_ip)
        app.run(host="0.0.0.0", port=int(port), debug=False)
    else:
        print("usage: python server.py <index> <ip_list_file>")
