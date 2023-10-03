#!/bin/bash
set +x
echo "--- check virtual environment"
[ -f venv/bin/activate ] || python3 -m venv venv
echo "--- activate virtual environemnt"
source ./venv/bin/activate
echo "--- update requirements"
python3 -m pip install --upgrade pip
python3 -m pip install -r requirements.txt
echo "--- execute script"
pytest scripts/ $*