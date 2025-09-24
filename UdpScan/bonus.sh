#!/bin/bash
PATTERN=$(echo -n '$group_7$' | xxd -p)
echo "Sending pattern: $PATTERN"
ping -c 1 -p $PATTERN 130.208.246.98
echo "Response received (if any echo reply came back)"
