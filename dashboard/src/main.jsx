import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import App from './App.jsx'

// Minimal global reset
document.body.style.cssText = 'margin:0;padding:0;box-sizing:border-box;'

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <App />
  </StrictMode>
)
