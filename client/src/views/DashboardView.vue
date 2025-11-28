<script setup>
import { ref, onMounted, onUnmounted } from 'vue' // nextTick não é mais necessário para lucide
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

// --- FUNÇÕES AUXILIARES ---

// Formata data ISO para horário local
const formatarHora = (isoString) => {
  if (!isoString) return '--:--'
  const data = new Date(isoString)
  return isNaN(data.getTime()) ? '--:--' : data.toLocaleTimeString('pt-BR')
}

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
  <div
    class="min-h-screen bg-neutral-50 text-neutral-900 selection:bg-black selection:text-white font-sans"
  >
    <div class="max-w-5xl mx-auto border-x border-neutral-200 min-h-screen flex flex-col">
      <!-- CABEÇALHO -->
      <header
        class="border-b border-neutral-200 p-8 flex flex-col md:flex-row md:items-end justify-between gap-6"
      >
        <div>
          <h1 class="text-4xl md:text-5xl font-bold tracking-tighter leading-none text-black">
            Dashboard
          </h1>
        </div>
      </header>

      <!-- PAINEL PRINCIPAL -->
      <main class="grid grid-cols-1 md:grid-cols-12 border-b border-neutral-200">
        <!-- Info do Veículo -->
        <div
          class="md:col-span-4 p-8 border-b md:border-b-0 md:border-r border-neutral-200 flex flex-col justify-between min-h-[240px]"
        >
          <div class="flex items-center gap-2 text-neutral-700 mb-2">
            <Bus class="w-4 h-4" />
            <span class="font-mono text-xs uppercase font-medium tracking-widest"
              >Veículo Ativo</span
            >
          </div>
          <div>
            <div class="text-6xl font-bold tracking-tighter">{{ veiculo.id }}</div>
            <div class="text-2xl text-neutral-800 font-medium tracking-tight mt-2">
              Rota {{ veiculo.rota }}
            </div>
          </div>
          <div class="mt-4">
            <span class="inline-block px-3 py-1 bg-black text-white text-xs font-mono uppercase"
              >Em Operação</span
            >
          </div>
        </div>

        <!-- Próxima Parada -->
        <div
          class="md:col-span-5 p-8 border-b md:border-b-0 md:border-r border-neutral-200 flex flex-col justify-center"
        >
          <div class="flex items-center gap-2 mb-4 text-neutral-700">
            <ArrowRight class="w-4 h-4" />
            <span class="font-mono text-xs uppercase tracking-widest">Próxima Parada</span>
          </div>
          <h2 class="text-3xl md:text-4xl font-medium leading-tight">
            {{ localizacao.proxima_parada?.nome || localizacao.proxima_parada?.id || '--' }}
          </h2>
          <div class="mt-6 pt-6 border-t border-neutral-100 flex items-center gap-2">
            <MapPin class="w-4 h-4 text-neutral-400" />
            <div>
              <span class="font-mono text-xs text-neutral-700 block mb-1">Origem Atual</span>
              <span class="text-lg">
                {{ localizacao.parada_atual?.nome || localizacao.parada_atual?.id || '--' }}
              </span>
            </div>
          </div>
        </div>

        <!-- Métricas -->
        <div class="md:col-span-3 grid grid-rows-2">
          <div class="p-8 border-b border-neutral-200 flex flex-col justify-center">
            <div class="flex items-center gap-2 mb-2 text-neutral-700">
              <Clock class="w-4 h-4" />
              <span class="font-mono text-xs uppercase">Estimativa</span>
            </div>
            <div class="flex items-baseline gap-1">
              <span
                class="text-4xl font-bold"
                :class="{ 'text-red-600': Number(estimativas.tempo_min) <= 2 }"
              >
                {{ estimativas.tempo_min }}
              </span>
              <span class="text-sm text-neutral-700 font-mono">min</span>
            </div>
          </div>
          <div class="p-8 flex flex-col justify-center bg-neutral-100">
            <div class="flex items-center gap-2 mb-2 text-neutral-700">
              <Ruler class="w-4 h-4" />
              <span class="font-mono text-xs uppercase">Distância</span>
            </div>
            <div class="flex items-baseline gap-1">
              <span class="text-3xl font-medium">{{ estimativas.distancia_km }}</span>
              <span class="text-sm text-neutral-700 font-mono">km</span>
            </div>
          </div>
        </div>
      </main>

      <!-- LISTA DE HISTÓRICO -->
      <section class="flex-1 flex flex-col">
        <div
          class="p-4 border-b border-neutral-200 bg-neutral-50 flex justify-between items-center"
        >
          <h3 class="font-mono text-xs uppercase tracking-widest text-neutral-800">
            Log de Eventos
          </h3>
          <button
            @click="limparTabela"
            class="text-xs font-mono underline decoration-neutral-300 hover:text-black hover:decoration-black transition-all cursor-pointer"
          >
            [Limpar Histórico]
          </button>
        </div>

        <div class="overflow-x-auto">
          <table class="w-full text-left text-sm">
            <thead class="font-mono text-xs uppercase text-neutral-800 border-b border-neutral-200">
              <tr>
                <th class="px-6 py-4 font-normal w-32">Horário</th>
                <th class="px-6 py-4 font-normal">Localização Atual</th>
                <th class="px-6 py-4 font-normal text-right">Destino</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-neutral-100">
              <tr v-if="historico.length === 0">
                <td colspan="3" class="px-6 py-6 text-center text-sm text-neutral-600 font-mono">
                  <div class="flex flex-col items-center justify-center gap-2">
                    <History class="w-8 h-8 text-neutral-500" />
                    <span>Nenhum histórico disponível no momento.</span>
                  </div>
                </td>
              </tr>
              <tr
                v-else
                v-for="(item, index) in historico"
                :key="index"
                class="group hover:bg-neutral-50 transition-colors"
              >
                <td class="px-6 py-4 font-mono text-xs text-neutral-800">
                  {{ formatarHora(item.timestamp) }}
                </td>
                <td class="px-6 py-4 font-medium text-neutral-900">
                  {{ item.localizacao.parada_atual?.nome || item.localizacao.parada_atual?.id }}
                </td>
                <td class="px-6 py-4 text-neutral-800 text-right">
                  {{ item.localizacao.proxima_parada?.nome || item.localizacao.proxima_parada?.id }}
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>
    </div>
  </div>
</template>
