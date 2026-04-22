# backend/conftest.py
# Makes `pytest` discoverable from the backend directory.
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
