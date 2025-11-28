import axios from 'axios'

const http = axios.create({
  baseURL: 'http://localhost:3000/api',
  timeout: 5000,
})

// Métodos da API
export const api = {
  getLatest: () => http.get('/telemetria/latest'),
  getHistorico: () => http.get('/telemetria/historico'),
  clearHistorico: () => http.delete('/telemetria/historico'),
}

export default http
