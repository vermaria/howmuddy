/**
 * dashboard/src/hooks/useOccupancy.js
 * Polls the backend every POLL_MS and returns live occupancy data.
 */
import { useState, useEffect, useCallback } from 'react'
import { fetchStatus, fetchWait, fetchHistory } from '../utils/api'

const POLL_MS = 5000   // 5-second refresh — matches firmware upload interval

export function useOccupancy(historyMinutes = 60) {
  const [tables,  setTables]  = useState([])
  const [wait,    setWait]    = useState(null)
  const [history, setHistory] = useState([])
  const [loading, setLoading] = useState(true)
  const [error,   setError]   = useState(null)
  const [lastUpdated, setLastUpdated] = useState(null)

  const refresh = useCallback(async () => {
    try {
      const [statusData, waitData, histData] = await Promise.all([
        fetchStatus(),
        fetchWait(),
        fetchHistory(historyMinutes),
      ])
      setTables(statusData.tables ?? [])
      setWait(waitData)
      setHistory(histData.data ?? [])
      setLastUpdated(new Date())
      setError(null)
    } catch (err) {
      setError(err.message)
    } finally {
      setLoading(false)
    }
  }, [historyMinutes])

  useEffect(() => {
    refresh()
    const id = setInterval(refresh, POLL_MS)
    return () => clearInterval(id)
  }, [refresh])

  return { tables, wait, history, loading, error, lastUpdated, refresh }
}
