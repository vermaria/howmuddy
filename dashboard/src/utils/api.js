/**
 * dashboard/src/utils/api.js
 * Thin wrappers around the How Muddy? REST API.
 */

const BASE = import.meta.env.VITE_API_BASE ?? '/api'

async function get(path) {
  const res = await fetch(`${BASE}${path}`)
  if (!res.ok) throw new Error(`GET ${path} → ${res.status}`)
  return res.json()
}

/** Full pub status: all tables + chairs */
export const fetchStatus = () => get('/status')

/** Estimated wait time for the whole pub */
export const fetchWait = () => get('/wait')

/** Occupancy history (last N minutes) */
export const fetchHistory = (minutes = 60) => get(`/history?minutes=${minutes}`)

/** Pub layout (table list with coords) */
export const fetchTables = () => get('/tables')
