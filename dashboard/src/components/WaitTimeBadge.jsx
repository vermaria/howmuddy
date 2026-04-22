/**
 * WaitTimeBadge.jsx
 * Prominent wait-time display at the top of the dashboard.
 */
export default function WaitTimeBadge({ wait }) {
  if (!wait) return null

  const { estimated_wait_min, confidence, occupancy_ratio } = wait
  const pct = Math.round(occupancy_ratio * 100)

  const color =
    pct >= 85 ? '#dc2626' :   // red — packed
    pct >= 55 ? '#f59e0b' :   // amber — busy
                '#16a34a'     // green — available

  const label =
    pct >= 85 ? 'Very Busy' :
    pct >= 55 ? 'Busy' :
    pct >= 20 ? 'Moderate' :
                'Open'

  return (
    <div style={{
      background: color,
      color: '#fff',
      borderRadius: 16,
      padding: '24px 40px',
      display: 'inline-flex',
      flexDirection: 'column',
      alignItems: 'center',
      gap: 4,
      boxShadow: '0 4px 20px rgba(0,0,0,0.15)',
      minWidth: 220,
    }}>
      <span style={{ fontSize: 13, fontWeight: 600, letterSpacing: 1, opacity: 0.85,
                     textTransform: 'uppercase' }}>
        Estimated Wait
      </span>
      <span style={{ fontSize: 52, fontWeight: 800, lineHeight: 1 }}>
        {estimated_wait_min === 0 ? '< 1' : estimated_wait_min}
        <span style={{ fontSize: 22, fontWeight: 400, marginLeft: 4 }}>min</span>
      </span>
      <span style={{ fontSize: 16, fontWeight: 600 }}>{label}</span>
      <span style={{ fontSize: 12, opacity: 0.75, marginTop: 2 }}>
        {pct}% full · {confidence} confidence
      </span>
    </div>
  )
}
