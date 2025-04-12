from node import Node
from node import FOLLOWER, LEADER
from auth import verify_jwt
from flask import Flask, request, jsonify
import sys
import logging
import json

app = Flask(__name__)

# value_get is the flask handle
@app.route("/request", methods=["GET"])
def value_get():
    payload = request.json["payload"]
    reply = {"code": "fail", "payload": payload}
    if n.status == LEADER:
        # request handle, reply is a dictionary
        result = n.handle_get(payload)
        if result:
            reply = {"code": "success", "payload": result}
    elif n.status == FOLLOWER:
        # redirect request
        reply["payload"]["message"] = n.leader
    return jsonify(reply)


@app.route("/request", methods=["PUT"])
def value_put():
    payload = request.json["payload"]
    reply = {"code": "fail"}

    if n.status == LEADER:
        new_payload = payload.copy()
        new_payload["value"] = json.loads(new_payload["value"])
        token = new_payload["value"].get("token")

        userID = verify_jwt(token)
        if userID is None:
            reply = {"code": "fail", "message": "Invalid token"}
            return jsonify(reply)
        
        existing_payload = n.handle_get(payload)

        if (
            existing_payload is not None
            and existing_payload["value"] != "__DELETE__"
            and new_payload["value"] != "__DELETE__"
        ):

            existing_value = json.loads(existing_payload["value"])
            existing_version = int(existing_value["version_number"])

            new_version = int(new_payload["value"]["version_number"])

            print("-->", existing_version, " -- ", new_version)
            if existing_version != new_version:
                reply = {"code": "fail", "message": "Version mismatch"}
                return jsonify(reply)
            else:
                new_version += 1
                new_payload["value"]["version_number"] = str(new_version)

            new_payload["value"] = json.dumps(new_payload["value"])

        result = n.handle_put(new_payload)
        if result:
            reply = {"code": "success", "payload": new_payload}
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
