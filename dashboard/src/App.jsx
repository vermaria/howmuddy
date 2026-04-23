/**
 * App.jsx
 * How Muddy? — Main dashboard application.
 *
 * Layout:
 *   ┌──────────────────────────────────────────┐
 *   │  Header: title + last-updated            │
 *   ├──────────────────────────────────────────┤
 *   │  Wait-Time Badge     │  Summary stats     │
 *   ├──────────────────────────────────────────┤
 *   │  Seat Map (floor plan / grid)            │
 *   ├──────────────────────────────────────────┤
 *   │  Occupancy Trend (60-min area chart)     │
 *   └──────────────────────────────────────────┘
 */
import { useOccupancy } from './hooks/useOccupancy'
import WaitTimeBadge from './components/WaitTimeBadge'
import SeatMap       from './components/SeatMap'
import TrendChart    from './components/TrendChart'

const styles = {
  root: {
    fontFamily: "'Inter', system-ui, sans-serif",
    minHeight: '100vh',
    background: '#f1f5f9',
    color: '#0f172a',
  },
  header: {
    background: '#0f172a',
    color: '#f8fafc',
    padding: '20px 32px',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  title: { fontSize: 22, fontWeight: 800, letterSpacing: -0.5 },
  subtitle: { fontSize: 13, opacity: 0.6, marginTop: 2 },
  main: { maxWidth: 1100, margin: '0 auto', padding: '28px 24px', display: 'flex',
          flexDirection: 'column', gap: 28 },
  card: {
    background: '#fff',
    borderRadius: 16,
    padding: '24px 28px',
    boxShadow: '0 1px 4px rgba(0,0,0,0.07)',
  },
  cardTitle: { fontSize: 14, fontWeight: 700, color: '#6b7280',
               textTransform: 'uppercase', letterSpacing: 0.8, marginBottom: 16 },
  heroRow: { display: 'flex', gap: 24, flexWrap: 'wrap', alignItems: 'center' },
  statsGrid: {
    display: 'grid',
    gridTemplateColumns: 'repeat(3, 1fr)',
    gap: 16,
    flex: 1,
    minWidth: 240,
  },
  statBox: {
    background: '#f8fafc',
    borderRadius: 12,
    padding: '16px 18px',
    textAlign: 'center',
  },
  statValue: { fontSize: 28, fontWeight: 800 },
  statLabel: { fontSize: 12, color: '#6b7280', marginTop: 4 },
  errorBanner: {
    background: '#fef2f2', border: '1px solid #fca5a5',
    borderRadius: 10, padding: '12px 16px', color: '#dc2626',
    fontSize: 13,
  },
  lastUpdated: { fontSize: 12, opacity: 0.55 },
}

export default function App() {
  const { tables, wait, history, loading, error, lastUpdated, refresh } =
    useOccupancy(60)

  const totalSeats    = tables.reduce((s, t) => s + (t.total_seats    ?? 0), 0)
  const occupiedSeats = tables.reduce((s, t) => s + (t.occupied_seats ?? 0), 0)
  const freeSeats     = totalSeats - occupiedSeats

  const formatTime = (d) =>
    d ? d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
      : '—'

  return (
    <div style={styles.root}>
      {/* ── Header ── */}
      <header style={styles.header}>
        <div>
          <div style={styles.title}>How Muddy? 🍺</div>
          <div style={styles.subtitle}>Live occupancy of the Muddy Charles Pub @ MIT</div>
        </div>
        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: 6 }}>
          <div style={styles.lastUpdated}>
            Updated {formatTime(lastUpdated)}
          </div>
          <button
            onClick={refresh}
            style={{
              background: '#ffffff20', border: '1px solid #ffffff30', color: '#fff',
              borderRadius: 8, padding: '5px 14px', fontSize: 12, cursor: 'pointer',
            }}
          >
            ↻ Refresh
          </button>
        </div>
      </header>

      <main style={styles.main}>
        {/* ── Error ── */}
        {error && (
          <div style={styles.errorBanner}>
            ⚠️ Could not reach backend: {error}. Make sure the server is running at{' '}
            <code>localhost:5000</code>.
          </div>
        )}

        {/* ── Hero row: wait badge + summary stats ── */}
        <div style={styles.card}>
          <div style={styles.cardTitle}>Current Status</div>
          {loading ? (
            <div style={{ color: '#9ca3af', fontSize: 14 }}>Loading…</div>
          ) : (
            <div style={styles.heroRow}>
              <WaitTimeBadge wait={wait} />
              <div style={styles.statsGrid}>
                <StatBox value={freeSeats}    label="Free Seats"     color="#22c55e" />
                <StatBox value={occupiedSeats} label="Occupied Seats" color="#ef4444" />
                <StatBox value={tables.length} label="Tables"         color="#6366f1" />
              </div>
            </div>
          )}
        </div>

        {/* ── Seat Map ── */}
        <div style={styles.card}>
          <div style={styles.cardTitle}>Seat Map</div>
          {loading ? (
            <div style={{ color: '#9ca3af', fontSize: 14 }}>Loading…</div>
          ) : tables.length === 0 ? (
            <div style={{ color: '#9ca3af', fontSize: 14 }}>
              No tables registered yet. Seed the database or wait for gateway reports.
            </div>
          ) : (
            <SeatMap tables={tables} />
          )}
        </div>

        {/* ── Legend ── */}
        <div style={{ display: 'flex', gap: 20, fontSize: 13, color: '#6b7280' }}>
          <LegendItem color="#22c55e" label="Free" />
          <LegendItem color="#ef4444" label="Occupied" />
          <LegendItem color="#d1d5db" label="Offline" />
        </div>

        {/* ── Trend Chart ── */}
        <div style={styles.card}>
          <div style={styles.cardTitle}>Occupancy Trend — Last 60 Minutes</div>
          <TrendChart history={history} />
        </div>

        {/* ── Footer ── */}
        <div style={{ textAlign: 'center', fontSize: 12, color: '#9ca3af', paddingBottom: 16 }}>
          6.1820 Mobile and Sensor Computing · Spring 2026 | Ria Verma, Elaine Wang, Eileen Zu
        </div>
      </main>
    </div>
  )
}

function StatBox({ value, label, color }) {
  return (
    <div style={styles.statBox}>
      <div style={{ ...styles.statValue, color }}>{value}</div>
      <div style={styles.statLabel}>{label}</div>
    </div>
  )
}

function LegendItem({ color, label }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
      <div style={{ width: 14, height: 14, borderRadius: '50%', background: color }} />
      {label}
    </div>
  )
}
