/**
 * SeatMap.jsx
 * Visual floor-plan of the Muddy Pub showing tables at their approximate
 * positions.  x_pos/y_pos are fractional (0–1) layout coordinates from the DB.
 * Falls back to a simple grid if layout coords are missing.
 */
import TableCard from './TableCard'

export default function SeatMap({ tables }) {
  const hasCoords = tables.some(t => t.x_pos != null && t.y_pos != null)

  if (hasCoords) {
    return <PositionedMap tables={tables} />
  }
  return <GridMap tables={tables} />
}

/** Render tables at their DB-stored fractional positions */
function PositionedMap({ tables }) {
  return (
    <div style={{ position: 'relative', width: '100%', aspectRatio: '16/9',
                  background: '#f8fafc', border: '2px dashed #e2e8f0',
                  borderRadius: 16, overflow: 'hidden' }}>
      {/* Bar label */}
      <div style={{
        position: 'absolute', left: '38%', top: '10%',
        background: '#cbd5e1', borderRadius: 8, padding: '6px 18px',
        fontSize: 12, fontWeight: 600, color: '#475569', letterSpacing: 1,
        textTransform: 'uppercase',
      }}>
        Bar
      </div>

      {tables.map(table => {
        const x = (table.x_pos ?? 0.5) * 100
        const y = (table.y_pos ?? 0.5) * 100
        return (
          <div key={table.id} style={{
            position: 'absolute',
            left: `${x}%`, top: `${y}%`,
            transform: 'translate(-50%, -50%)',
            width: 'clamp(140px, 18%, 200px)',
          }}>
            <TableCard table={table} />
          </div>
        )
      })}
    </div>
  )
}

/** Fallback grid layout */
function GridMap({ tables }) {
  return (
    <div style={{
      display: 'grid',
      gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))',
      gap: 16,
    }}>
      {tables.map(table => (
        <TableCard key={table.id} table={table} />
      ))}
    </div>
  )
}
