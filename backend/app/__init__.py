"""
backend/app/__init__.py
Flask application factory.
"""
import os
from flask import Flask
from flask_sqlalchemy import SQLAlchemy
from flask_cors import CORS

db = SQLAlchemy()


def create_app(env: str = None) -> Flask:
    """Create and configure the Flask application."""
    from .config import config_map

    env = env or os.getenv("FLASK_ENV", "development")
    cfg = config_map.get(env, config_map["development"])

    app = Flask(__name__)
    app.config.from_object(cfg)

    # Extensions
    db.init_app(app)
    CORS(app, resources={r"/api/*": {"origins": "*"}})

    # Register blueprints
    from .routes import api_bp
    app.register_blueprint(api_bp, url_prefix="/api")

    # Create tables
    with app.app_context():
        db.create_all()

    return app
