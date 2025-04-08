#!/bin/bash

# Usage: ./start_cluster.sh <cluster_number>
# Example: ./start_cluster.sh 0  --> starts all servers using default Procfile
# Example: ./start_cluster.sh 2  --> starts servers from Procfile.cluster2

CLUSTER_NUM="$1"

if [[ -z "$CLUSTER_NUM" ]]; then
    echo "Usage: $0 <cluster_number>"
    echo "Example: $0 0  --> starts all servers using default Procfile"
    echo "Example: $0 x  --> starts servers from Procfile.cluster<x>"
    exit 1
fi

if [[ "$CLUSTER_NUM" -eq 0 ]]; then
    echo "Starting all servers using default Procfile..."
    foreman start
else
    PROCFILE="Procfile.cluster$CLUSTER_NUM"
    if [[ -f "$PROCFILE" ]]; then
        echo "Starting cluster $CLUSTER_NUM using $PROCFILE..."
        foreman start -f "$PROCFILE"
    else
        echo "Error: $PROCFILE does not exist."
        exit 1
    fi
fi
