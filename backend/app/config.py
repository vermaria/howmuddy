"""
backend/app/config.py
App configuration for development, testing, and production.
"""
import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent


class Config:
    SECRET_KEY = os.getenv("SECRET_KEY", "dev-secret-change-me")
    SQLALCHEMY_TRACK_MODIFICATIONS = False

    # Operating hours — reports outside these hours are flagged but still stored
    PUB_OPEN_HOUR  = int(os.getenv("PUB_OPEN_HOUR",  "11"))
    PUB_CLOSE_HOUR = int(os.getenv("PUB_CLOSE_HOUR", "23"))

    # Chair stale timeout: marks a chair offline if no report in this many seconds
    CHAIR_STALE_SECONDS = int(os.getenv("CHAIR_STALE_SECONDS", "30"))
    # Table stale timeout: marks a gateway offline if no report in this many seconds
    TABLE_STALE_SECONDS = int(os.getenv("TABLE_STALE_SECONDS", "30"))

    # Wait-time model parameters
    # Estimated average table turn time in minutes at the Muddy
    AVG_TABLE_TURN_MIN = float(os.getenv("AVG_TABLE_TURN_MIN", "45.0"))


class DevelopmentConfig(Config):
    DEBUG = True
    SQLALCHEMY_DATABASE_URI = f"sqlite:///{BASE_DIR / 'muddy_dev.db'}"


class TestingConfig(Config):
    TESTING = True
    SQLALCHEMY_DATABASE_URI = "sqlite:///:memory:"


class ProductionConfig(Config):
    DEBUG = False
    SQLALCHEMY_DATABASE_URI = os.getenv(
        "DATABASE_URL",
        f"sqlite:///{BASE_DIR / 'muddy_prod.db'}"
    )


config_map = {
    "development": DevelopmentConfig,
    "testing":     TestingConfig,
    "production":  ProductionConfig,
}
