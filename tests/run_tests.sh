#!/bin/bash
set +x
echo "--- activate virtual environemnt"
source ./venv/bin/activate
echo "--- update requirements"
pip3 install -r requirements.txt
echo "--- execute script"
pytest scripts/ $*