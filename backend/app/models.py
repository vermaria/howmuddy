"""
backend/app/models.py
SQLAlchemy ORM models for How Muddy?
"""
from datetime import datetime, timezone
from . import db


class Table(db.Model):
    __tablename__ = "tables"

    id       = db.Column(db.String(8),  primary_key=True)
    label    = db.Column(db.String(32), nullable=False)
    capacity = db.Column(db.Integer,    nullable=False, default=4)
    section  = db.Column(db.String(16), nullable=True)
    x_pos    = db.Column(db.Float,      nullable=True)
    y_pos    = db.Column(db.Float,      nullable=True)

    chairs  = db.relationship("Chair",       back_populates="table", lazy="dynamic")
    reports = db.relationship("ChairReport", back_populates="table", lazy="dynamic",
                              foreign_keys="ChairReport.table_id")

    def occupied_count(self):
        return sum(1 for c in self.chairs if c.is_occupied)

    def to_dict(self):
        chairs = list(self.chairs)
        return {
            "id":             self.id,
            "label":          self.label,
            "capacity":       self.capacity,
            "section":        self.section,
            "x_pos":          self.x_pos,
            "y_pos":          self.y_pos,
            "occupied_seats": self.occupied_count(),
            "total_seats":    len(chairs),
            "chairs":         [c.to_dict() for c in chairs],
        }


class Chair(db.Model):
    __tablename__ = "chairs"

    id          = db.Column(db.String(8),  primary_key=True)
    table_id    = db.Column(db.String(8),  db.ForeignKey("tables.id"), nullable=True)
    is_occupied = db.Column(db.Boolean,    nullable=False, default=False)
    last_rssi   = db.Column(db.Integer,    nullable=True)
    last_adc    = db.Column(db.Integer,    nullable=True)
    battery_pct = db.Column(db.Integer,    nullable=True)
    last_seen   = db.Column(db.DateTime,   nullable=True)
    is_online   = db.Column(db.Boolean,    nullable=False, default=False)

    table   = db.relationship("Table",       back_populates="chairs",
                              foreign_keys=[table_id])
    reports = db.relationship("ChairReport", back_populates="chair", lazy="dynamic",
                              foreign_keys="ChairReport.chair_id")

    def to_dict(self):
        return {
            "id":          self.id,
            "table_id":    self.table_id,
            "is_occupied": self.is_occupied,
            "last_rssi":   self.last_rssi,
            "battery_pct": self.battery_pct,
            "is_online":   self.is_online,
            "last_seen":   self.last_seen.isoformat() if self.last_seen else None,
        }


class ChairReport(db.Model):
    """Immutable time-series record of one occupancy report."""
    __tablename__ = "chair_reports"

    id           = db.Column(db.Integer,  primary_key=True, autoincrement=True)
    chair_id     = db.Column(db.String(8), db.ForeignKey("chairs.id"),  nullable=False)
    table_id     = db.Column(db.String(8), db.ForeignKey("tables.id"),  nullable=False)
    occupied     = db.Column(db.Boolean,   nullable=False)
    rssi         = db.Column(db.Integer,   nullable=True)
    adc_raw      = db.Column(db.Integer,   nullable=True)
    battery_pct  = db.Column(db.Integer,   nullable=True)
    ts           = db.Column(db.DateTime,  nullable=False,
                             default=lambda: datetime.now(timezone.utc))
    during_hours = db.Column(db.Boolean,   nullable=False, default=True)

    chair = db.relationship("Chair", back_populates="reports",
                            foreign_keys=[chair_id])
    table = db.relationship("Table", back_populates="reports",
                            foreign_keys=[table_id])

    def to_dict(self):
        return {
            "id":           self.id,
            "chair_id":     self.chair_id,
            "table_id":     self.table_id,
            "occupied":     self.occupied,
            "rssi":         self.rssi,
            "adc_raw":      self.adc_raw,
            "battery_pct":  self.battery_pct,
            "ts":           self.ts.isoformat(),
            "during_hours": self.during_hours,
        }
