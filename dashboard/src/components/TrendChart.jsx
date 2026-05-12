// /**
//  * TrendChart.jsx
//  * Recharts area chart showing occupancy ratio over the last N minutes.
//  */
// import {
//   AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip,
//   ResponsiveContainer, ReferenceLine,
// } from 'recharts'

// function formatTime(isoString) {
//   const d = new Date(isoString)
//   return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
// }

// const CustomTooltip = ({ active, payload, label }) => {
//   if (!active || !payload?.length) return null
//   const d = payload[0].payload
//   return (
//     <div style={{
//       background: '#1f2937', color: '#f9fafb',
//       padding: '10px 14px', borderRadius: 10, fontSize: 13,
//     }}>
//       <div style={{ fontWeight: 600, marginBottom: 4 }}>{formatTime(label)}</div>
//       <div>Occupied: {d.occupied} / {d.total}</div>
//       <div>Ratio: {Math.round(d.ratio * 100)}%</div>
//     </div>
//   )
// }

// export default function TrendChart({ history }) {
//   if (!history || history.length === 0) {
//     return (
//       <div style={{
//         height: 180, display: 'flex', alignItems: 'center',
//         justifyContent: 'center', color: '#9ca3af', fontSize: 14,
//       }}>
//         No trend data yet — check back once the pub is open.
//       </div>
//     )
//   }

//   // Recharts needs plain numbers
//   const data = history.map(b => ({
//     ...b,
//     pct: Math.round(b.ratio * 100),
//   }))

//   return (
//     <ResponsiveContainer width="100%" height={220}>
//       <AreaChart data={data} margin={{ top: 8, right: 8, bottom: 0, left: -20 }}>
//         <defs>
//           <linearGradient id="occGrad" x1="0" y1="0" x2="0" y2="1">
//             <stop offset="5%"  stopColor="#6366f1" stopOpacity={0.3} />
//             <stop offset="95%" stopColor="#6366f1" stopOpacity={0.02} />
//           </linearGradient>
//         </defs>
//         <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
//         <XAxis
//           dataKey="ts"
//           tickFormatter={formatTime}
//           tick={{ fontSize: 11, fill: '#9ca3af' }}
//           interval="preserveStartEnd"
//         />
//         <YAxis
//           domain={[0, 100]}
//           tickFormatter={v => `${v}%`}
//           tick={{ fontSize: 11, fill: '#9ca3af' }}
//         />
//         <Tooltip content={<CustomTooltip />} />
//         <ReferenceLine y={85} stroke="#ef4444" strokeDasharray="4 3"
//                        label={{ value: 'Very Busy', fill: '#ef4444', fontSize: 10 }} />
//         <ReferenceLine y={55} stroke="#f59e0b" strokeDasharray="4 3"
//                        label={{ value: 'Busy', fill: '#f59e0b', fontSize: 10 }} />
//         <Area
//           type="monotone"
//           dataKey="pct"
//           stroke="#6366f1"
//           strokeWidth={2}
//           fill="url(#occGrad)"
//           dot={false}
//           activeDot={{ r: 4, fill: '#6366f1' }}
//         />
//       </AreaChart>
//     </ResponsiveContainer>
//   )
// }


import {
  AreaChart, Area, XAxis, YAxis, CartesianGrid, Tooltip,
  ResponsiveContainer, ReferenceLine,
} from 'recharts'

function formatTime(isoString) {
  const d = new Date(isoString)
  return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
}

const CustomTooltip = ({ active, payload, label }) => {
  if (!active || !payload?.length) return null
  const d = payload[0].payload
  return (
    <div style={{
      background: '#1f2937', color: '#f9fafb',
      padding: '10px 14px', borderRadius: 10, fontSize: 13,
    }}>
      <div style={{ fontWeight: 600, marginBottom: 4 }}>{formatTime(label)}</div>
      <div>Occupied: {d.occupied} / {d.total}</div>
      <div>Ratio: {Math.round(d.ratio * 100)}%</div>
    </div>
  )
}

// Custom label rendered on the LEFT side so it never overlaps the chart area
const LeftLabel = ({ viewBox, value, color }) => {
  const { y } = viewBox
  return (
    <text x={8} y={y - 4} fill={color} fontSize={10} fontWeight={600}>
      {value}
    </text>
  )
}

export default function TrendChart({ history }) {
  if (!history || history.length === 0) {
    return (
      <div style={{
        height: 180, display: 'flex', alignItems: 'center',
        justifyContent: 'center', color: '#9ca3af', fontSize: 14,
      }}>
        No trend data yet — check back once the pub is open.
      </div>
    )
  }

  const data = history.map(b => ({ ...b, pct: Math.round(b.ratio * 100) }))

  return (
    <ResponsiveContainer width="100%" height={240}>
      <AreaChart data={data} margin={{ top: 24, right: 16, bottom: 0, left: 0 }}>
        <defs>
          <linearGradient id="occGrad" x1="0" y1="0" x2="0" y2="1">
            <stop offset="5%"  stopColor="#6366f1" stopOpacity={0.3} />
            <stop offset="95%" stopColor="#6366f1" stopOpacity={0.02} />
          </linearGradient>
        </defs>
        <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
        <XAxis
          dataKey="ts"
          tickFormatter={formatTime}
          tick={{ fontSize: 11, fill: '#9ca3af' }}
          interval="preserveStartEnd"
        />
        <YAxis
          domain={[0, 100]}
          tickFormatter={v => `${v}%`}
          tick={{ fontSize: 11, fill: '#9ca3af' }}
          width={40}
        />
        <Tooltip content={<CustomTooltip />} />
        <ReferenceLine
          y={85}
          stroke="#ef4444"
          strokeDasharray="4 3"
          label={<LeftLabel value="Very Busy" color="#ef4444" />}
        />
        <ReferenceLine
          y={55}
          stroke="#f59e0b"
          strokeDasharray="4 3"
          label={<LeftLabel value="Busy" color="#f59e0b" />}
        />
        <Area
          type="monotone"
          dataKey="pct"
          stroke="#6366f1"
          strokeWidth={2}
          fill="url(#occGrad)"
          dot={false}
          activeDot={{ r: 4, fill: '#6366f1' }}
        />
      </AreaChart>
    </ResponsiveContainer>
  )
}
