<script setup>
import { ref, onMounted, onUnmounted, computed } from 'vue' // nextTick não é mais necessário para lucide
import { api } from '@/services/http'
import io from 'socket.io-client'
import { Bus, MapPin, Clock, Ruler, ArrowRight, History } from 'lucide-vue-next'

// --- ESTADO REATIVO (Inicializado com segurança) ---
const veiculo = ref({
  id: '--',
  rota: '--',
})

const localizacao = ref({
  parada_atual: { id: 0, nome: '--' },
  proxima_parada: { id: 0, nome: 'Aguardando...' },
})

const estimativas = ref({
  distancia_km: '--',
  tempo_min: '--',
})

const historico = ref([])
let socket = null // A variável socket precisa estar no escopo para onUnmounted
const ultimaAtualizacao = ref('--:--')

// --- FUNÇÕES AUXILIARES ---

// Formata data ISO para horário local
const formatarHora = (isoString) => {
  if (!isoString) return '--:--'
  const data = new Date(isoString)
  return isNaN(data.getTime()) ? '--:--' : data.toLocaleTimeString('pt-BR')
}

const formatarDistancia = (valor) => {
  if (valor === undefined || valor === null || valor === '--' || Number.isNaN(Number(valor))) {
    return '--'
  }
  return valor
}

const tempoCritico = computed(() => Number(estimativas.value.tempo_min) <= 2)

// Limpa o histórico visual e persiste exclusão no backend
const limparTabela = async () => {
  try {
    await api.clearHistorico()
    historico.value = []
  } catch (erro) {
    console.error('Erro ao limpar histórico:', erro)
    historico.value = []
  }
}

// Atualiza a interface com novos dados recebidos (cards principais e histórico)
const atualizarInterface = (dados) => {
  if (!dados) return

  // Proteção contra dados parciais
  if (dados.veiculo) veiculo.value = dados.veiculo
  if (dados.localizacao) localizacao.value = dados.localizacao
  if (dados.estimativas) estimativas.value = dados.estimativas

  // marca última atualização
  if (dados.timestamp) {
    ultimaAtualizacao.value = formatarHora(dados.timestamp)
  } else {
    ultimaAtualizacao.value = formatarHora(new Date().toISOString())
  }

  // Adiciona ao histórico apenas se for um novo evento de parada
  if (dados.novo_evento) {
    historico.value.unshift(dados)
    if (historico.value.length > 8) {
      historico.value.pop()
    }
  }
}

// --- CARGA INICIAL ---
const carregarHistoricoInicial = async () => {
  try {
    const { data } = await api.getHistorico()
    if (data && Array.isArray(data) && data.length > 0) {
      atualizarInterface(data[0]) // Atualiza cards com o dado mais recente
      historico.value = data.slice(0, 8) // Popula histórico visual
    }
  } catch (erro) {
    console.error('Erro ao carregar histórico inicial:', erro)
  }
}

// --- CICLO DE VIDA DO COMPONENTE ---
onMounted(() => {
  // 1. Carrega dados iniciais via HTTP
  carregarHistoricoInicial()

  // 2. Conecta ao Socket.IO para atualizações em Tempo Real
  socket = io('http://localhost:3000', {
    transports: ['websocket'], // Força WebSocket para evitar polling/recargas
    autoConnect: true,
    reconnection: true,
  })

  socket.on('connect', () => {
    // console.log('✅ Conectado ao servidor de tempo real') // Log opcional
  })

  socket.on('disconnect', () => {
    // console.log('❌ Desconectado do servidor') // Log opcional
  })

  socket.on('connect_error', (error) => {
    console.error('⚠️ Erro de conexão Socket.IO:', error)
  })

  socket.on('telemetria-update', (dados) => {
    atualizarInterface(dados)
  })
})

onUnmounted(() => {
  if (socket) socket.disconnect()
})
</script>

<template>
  <div class="page">
    <header class="block">
      <h1>Dashboard de Telemetria</h1>
      <p class="small">
        Última atualização: <span class="mono">{{ ultimaAtualizacao }}</span>
      </p>
    </header>

    <section class="section">
      <div class="cols">
        <div class="card">
          <div class="card-head">
            <Bus class="icon" />
            <span>Veículo ativo</span>
          </div>
          <div class="big">{{ veiculo.id }}</div>
          <div class="muted">Rota {{ veiculo.rota }}</div>
          <div class="tag">Em operação</div>
        </div>

        <div class="card">
          <div class="card-head">
            <ArrowRight class="icon" />
            <span>Próxima parada</span>
          </div>
          <div class="title">
            {{ localizacao.proxima_parada?.nome || localizacao.proxima_parada?.id || '--' }}
          </div>
          <div class="row">
            <MapPin class="icon small" />
            <div>
              <div class="label">Origem atual</div>
              <div>
                {{ localizacao.parada_atual?.nome || localizacao.parada_atual?.id || '--' }}
              </div>
            </div>
          </div>
        </div>

        <div class="card grid-two">
          <div>
            <div class="label row">
              <Clock class="icon small" />
              <span>Estimativa</span>
            </div>
            <div class="big" :class="{ alert: tempoCritico }">{{ estimativas.tempo_min }}</div>
            <div class="muted">minutos</div>
          </div>
          <div>
            <div class="label row">
              <Ruler class="icon small" />
              <span>Distância</span>
            </div>
            <div class="big">{{ formatarDistancia(estimativas.distancia_km) }}</div>
            <div class="muted">km</div>
          </div>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-head">
        <h2>Histórico</h2>
        <button class="link-line" @click="limparTabela">Limpar histórico</button>
      </div>
      <div class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Horário</th>
              <th>Localização Atual</th>
              <th>Destino</th>
            </tr>
          </thead>
          <tbody>
            <tr v-if="historico.length === 0">
              <td colspan="3" class="empty">
                <div class="empty-content">
                  <History class="icon" />
                  <span>Nenhum histórico disponível.</span>
                </div>
              </td>
            </tr>
            <tr v-else v-for="(item, index) in historico" :key="index">
              <td class="mono">{{ formatarHora(item.timestamp) }}</td>
              <td>
                {{ item.localizacao.parada_atual?.nome || item.localizacao.parada_atual?.id }}
              </td>
              <td>
                {{ item.localizacao.proxima_parada?.nome || item.localizacao.proxima_parada?.id }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </section>
  </div>
</template>

<style scoped>
@import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600&display=swap');

.page {
  --paper: #f6f2e9;
  --ink: #1f1f1f;
  --line: #d8d2c7;
  --muted: #555;
  min-height: 100vh;
  margin: 0 auto;
  max-width: 960px;
  padding: 32px 20px 48px;
  background: var(--paper);
  color: var(--ink);
  font-family: 'IBM Plex Mono', 'Courier New', monospace;
  letter-spacing: 0.01em;
  font-size: 16px;
}
header.block {
  border: 1px solid var(--line);
  padding: 16px;
  background: #fbf9f5;
}
h1 {
  margin: 0;
  font-size: 24px;
}
header p {
  margin: 6px 0 0;
  color: var(--muted);
  font-size: 14px;
}
.small {
  margin: 4px 0 0;
  color: var(--muted);
  font-size: 13px;
}
.section {
  border: 1px solid var(--line);
  padding: 16px;
  margin-top: 18px;
  background: #fff;
}
.section-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
}
h2 {
  margin: 0;
  font-size: 17px;
  letter-spacing: 0.05em;
  text-transform: uppercase;
}
.link-line {
  border: 1px solid var(--line);
  background: #faf8f4;
  padding: 6px 10px;
  font-weight: 600;
  cursor: pointer;
}
.cols {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 12px;
}
.card {
  border: 1px solid var(--line);
  padding: 12px;
  background: #faf8f4;
  display: flex;
  flex-direction: column;
  gap: 6px;
  min-height: 140px;
}
.grid-two {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
}
.card-head {
  display: flex;
  align-items: center;
  gap: 8px;
  text-transform: uppercase;
  font-size: 13px;
  letter-spacing: 0.08em;
}
.icon {
  width: 16px;
  height: 16px;
}
.icon.small {
  width: 14px;
  height: 14px;
}
.big {
  font-size: 28px;
  font-weight: 700;
}
.title {
  font-size: 20px;
  font-weight: 600;
}
.muted {
  color: var(--muted);
  font-size: 13px;
}
.label {
  color: var(--muted);
  font-size: 12px;
  letter-spacing: 0.05em;
  text-transform: uppercase;
}
.row {
  display: flex;
  align-items: center;
  gap: 6px;
}
.tag {
  display: inline-block;
  padding: 4px 8px;
  border: 1px solid var(--line);
  font-size: 12px;
  width: fit-content;
}
.alert {
  color: #b62323;
}
.table-wrap {
  margin-top: 12px;
  border: 1px solid var(--line);
  overflow-x: auto;
  background: #faf8f4;
}
table {
  width: 100%;
  border-collapse: collapse;
  font-size: 14px;
}
thead {
  background: #fbf9f5;
}
th,
td {
  border-bottom: 1px solid var(--line);
  padding: 10px;
}
tbody tr:hover {
  background: #f1ece1;
}
th {
  text-align: left;
  text-transform: uppercase;
  font-size: 13px;
  letter-spacing: 0.08em;
}
.right {
  text-align: right;
}
.mono {
  font-family: 'IBM Plex Mono', 'Courier New', monospace;
}
.empty {
  text-align: center;
  color: var(--muted);
  padding: 28px 20px;
}
.empty-content {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  flex-direction: column;
}
@media (max-width: 640px) {
  .page {
    padding: 20px 14px 32px;
  }
  .grid-two {
    grid-template-columns: 1fr;
  }
}
</style>
