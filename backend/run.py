"""
backend/run.py
Entry point for running the How Muddy? backend.

Usage:
    python run.py               # development (debug=True, port 5000)
    FLASK_ENV=production python run.py
"""
import os
from app import create_app

app = create_app(os.getenv("FLASK_ENV", "development"))

if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=int(os.getenv("PORT", 5000)),
        debug=app.config.get("DEBUG", False),
    )
