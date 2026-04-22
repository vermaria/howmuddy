/**
 * TableCard.jsx
 * Displays occupancy for a single table with chair-level detail.
 */
export default function TableCard({ table }) {
  const { id, label, chairs = [], occupied_seats, total_seats, section } = table
  const ratio = total_seats > 0 ? occupied_seats / total_seats : 0
  const available = total_seats - occupied_seats

  const bgColor =
    ratio >= 0.85 ? '#fef2f2' :
    ratio >= 0.5  ? '#fffbeb' :
                    '#f0fdf4'

  const borderColor =
    ratio >= 0.85 ? '#fca5a5' :
    ratio >= 0.5  ? '#fcd34d' :
                    '#86efac'

  return (
    <div style={{
      background: bgColor,
      border: `2px solid ${borderColor}`,
      borderRadius: 14,
      padding: '18px 20px',
      display: 'flex',
      flexDirection: 'column',
      gap: 12,
      minWidth: 200,
    }}>
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
        <div>
          <div style={{ fontWeight: 700, fontSize: 16 }}>{label}</div>
          {section && (
            <div style={{ fontSize: 11, color: '#6b7280', textTransform: 'uppercase',
                          letterSpacing: 0.5, marginTop: 2 }}>
              {section}
            </div>
          )}
        </div>
        <div style={{ textAlign: 'right' }}>
          <div style={{ fontWeight: 700, fontSize: 20 }}>
            {available}
            <span style={{ fontSize: 12, fontWeight: 400, color: '#6b7280' }}> free</span>
          </div>
          <div style={{ fontSize: 11, color: '#6b7280' }}>
            {occupied_seats}/{total_seats} seats
          </div>
        </div>
      </div>

      {/* Chair dots */}
      <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
        {chairs.length === 0 ? (
          <span style={{ fontSize: 12, color: '#9ca3af' }}>No chairs online</span>
        ) : (
          chairs.map(chair => (
            <ChairDot key={chair.id} chair={chair} />
          ))
        )}
      </div>

      {/* Occupancy bar */}
      <div style={{ height: 6, background: '#e5e7eb', borderRadius: 99, overflow: 'hidden' }}>
        <div style={{
          height: '100%',
          width: `${ratio * 100}%`,
          background: ratio >= 0.85 ? '#ef4444' : ratio >= 0.5 ? '#f59e0b' : '#22c55e',
          borderRadius: 99,
          transition: 'width 0.4s ease',
        }} />
      </div>
    </div>
  )
}

function ChairDot({ chair }) {
  const { is_occupied, is_online, id, battery_pct } = chair

  const bg =
    !is_online   ? '#d1d5db' :
    is_occupied  ? '#ef4444' :
                   '#22c55e'

  const label =
    !is_online  ? 'offline' :
    is_occupied ? 'occupied' :
                  'free'

  return (
    <div title={`${id} · ${label}${battery_pct != null ? ` · 🔋${battery_pct}%` : ''}`}
         style={{
           width: 28, height: 28,
           borderRadius: '50%',
           background: bg,
           display: 'flex', alignItems: 'center', justifyContent: 'center',
           fontSize: 12,
           boxShadow: '0 1px 3px rgba(0,0,0,0.12)',
           cursor: 'default',
           transition: 'background 0.3s ease',
         }}>
      {!is_online ? '·' : is_occupied ? '●' : '○'}
    </div>
  )
}
