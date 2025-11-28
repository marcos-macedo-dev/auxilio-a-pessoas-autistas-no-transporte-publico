require('dotenv').config();
const mqtt = require('mqtt');
const express = require('express');
const cors = require('cors');
const path = require('path');
const http = require('http');
const socketIo = require('socket.io');
const fs = require('fs').promises;

// --- CONFIGURAÇÃO ---
const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: "*", 
    methods: ["GET", "POST"],
    credentials: true
  },
  pingTimeout: 60000, // Aumenta tolerância para 60s
  pingInterval: 25000, // Envia ping a cada 25s
  connectionStateRecovery: {
    // Ajuda a recuperar a sessão se a conexão cair brevemente
    maxDisconnectionDuration: 2 * 60 * 1000,
    skipMiddlewares: true,
  }
});

const PORT = process.env.PORT || 3000;
const MAX_HISTORICO = 100; // Limite de registros no JSON
const ARQUIVO_DADOS = path.join(__dirname, 'data', 'telemetria.json');

// --- MIDDLEWARE ---
app.use(cors());
app.use(express.json());

// --- ARMAZENAMENTO EM MEMÓRIA (CACHE) ---
let dadosTelemetria = [];

// --- FUNÇÕES AUXILIARES (PERSISTÊNCIA) ---

// Carrega dados do disco na inicialização
async function carregarDadosIniciais() {
  try {
    const dir = path.dirname(ARQUIVO_DADOS);
    await fs.mkdir(dir, { recursive: true });

    try {
      const data = await fs.readFile(ARQUIVO_DADOS, 'utf8');
      dadosTelemetria = JSON.parse(data);
      console.log(`📦 Dados carregados: ${dadosTelemetria.length} registros.`);
    } catch (err) {
      if (err.code === 'ENOENT') {
        console.log('⚠️ Arquivo não encontrado. Criando novo...');
        await fs.writeFile(ARQUIVO_DADOS, JSON.stringify([], null, 2));
        dadosTelemetria = [];
      } else {
        throw err;
      }
    }
  } catch (error) {
    console.error('❌ Erro ao carregar dados:', error.message);
    dadosTelemetria = [];
  }
}

// Salva dados da memória para o disco
async function persistirDados() {
  try {
    await fs.writeFile(ARQUIVO_DADOS, JSON.stringify(dadosTelemetria, null, 2));
  } catch (error) {
    console.error('❌ Erro ao salvar arquivo:', error.message);
  }
}

// Validação básica dos dados recebidos
function validarDadosTelemetria(dados) {
  // Campos essenciais. 'tempo_min' e 'nomes' são opcionais para maior resiliência.
  const obrigatorios = ['id_onibus', 'id_rota', 'id_parada', 'id_proxima_parada', 'distancia_km'];

  for (const campo of obrigatorios) {
    if (dados[campo] === undefined || dados[campo] === null) {
      throw new Error(`Campo obrigatório ausente: ${campo}`);
    }
  }

  if (typeof dados.id_onibus !== 'number' || dados.id_onibus <= 0) {
    throw new Error('ID do ônibus inválido.');
  }

  return true;
}

// Processa e armazena um novo registro
async function salvarTelemetria(dados) {
  try {
    dados.timestamp = new Date(); // Adiciona data/hora do servidor
    
    // Adiciona ao início da lista
    dadosTelemetria.unshift(dados);

    // Aplica limite de tamanho
    if (dadosTelemetria.length > MAX_HISTORICO) {
      dadosTelemetria = dadosTelemetria.slice(0, MAX_HISTORICO);
    }

    console.log('✅ Dados salvos em JSON/Memória.');
    await persistirDados();
    return true;
  } catch (error) {
    console.error('❌ Erro ao processar salvamento:', error.message);
    return false;
  }
}

// --- MQTT (COMUNICAÇÃO IOT) ---

const opcoesMqtt = {
  host: '37c8059a614a4b1caf32c484a8ad7f41.s1.eu.hivemq.cloud',
  port: 8883,
  protocol: 'mqtts',
  username: 'marquin',
  password: 'Senha123',
  clientId: 'servidor-' + Math.random().toString(16).substr(2, 8),
  clean: true,
  reconnectPeriod: 5000,
  connectTimeout: 30000
};

const clienteMqtt = mqtt.connect(opcoesMqtt);

clienteMqtt.on('connect', () => {
  console.log('📡 Conectado ao Broker MQTT (HiveMQ)');
  clienteMqtt.subscribe('onibus/telemetria', { qos: 1 }, (err) => {
    if (!err) console.log('👂 Ouvindo tópico: onibus/telemetria');
  });
});

clienteMqtt.on('error', (erro) => console.error('❌ Erro MQTT:', erro));

clienteMqtt.on('message', async (topico, mensagem) => {
  try {
    const dadosBrutos = JSON.parse(mensagem.toString());
    // console.log('📨 Msg MQTT:', dadosBrutos); // Log reduzido

    validarDadosTelemetria(dadosBrutos);

    // Estimativa de tempo (Fallback se não vier do dispositivo)
    let tempoEstimado = dadosBrutos.tempo_min;
    if (tempoEstimado === undefined || tempoEstimado === null) {
       tempoEstimado = Math.ceil(dadosBrutos.distancia_km * 2); 
    }

    // Estrutura padronizada
    const dadosFormatados = {
      veiculo: {
        id: dadosBrutos.id_onibus,
        rota: dadosBrutos.id_rota
      },
      localizacao: {
        parada_atual: {
          id: dadosBrutos.id_parada,
          nome: dadosBrutos.nome_parada || `Parada ${dadosBrutos.id_parada}`
        },
        proxima_parada: {
          id: dadosBrutos.id_proxima_parada,
          nome: dadosBrutos.nome_proxima_parada || `Parada ${dadosBrutos.id_proxima_parada}`
        }
      },
      estimativas: {
        distancia_km: dadosBrutos.distancia_km,
        tempo_min: tempoEstimado
      }
    };

    // Verifica se é um NOVO EVENTO (mudança de parada)
    const ultimo = dadosTelemetria[0];
    let ehNovoEvento = true;

    if (ultimo) {
      const mesmaParadaAtual = dadosFormatados.localizacao.parada_atual.id === ultimo.localizacao.parada_atual.id;
      const mesmaProximaParada = dadosFormatados.localizacao.proxima_parada.id === ultimo.localizacao.proxima_parada.id;
      
      if (mesmaParadaAtual && mesmaProximaParada) {
        ehNovoEvento = false;
      }
    }

    dadosFormatados.novo_evento = ehNovoEvento;

    // SEMPRE SALVA O ESTADO MAIS RECENTE (Para atualizar o 'latest' da API)
    // Mas a lógica de "adicionar ao histórico" pode ser tratada aqui ou apenas a flag.
    // Para simplificar: Vamos salvar TUDO no array, mas o frontend filtra o histórico.
    // OU melhor: Se não for novo evento, nós substituímos o índice 0 (update) em vez de unshift (append).
    
    if (ehNovoEvento) {
        console.log('📌 Novo evento de parada detectado.');
        await salvarTelemetria(dadosFormatados);
    } else {
        // Atualiza apenas o registro mais recente
        if (dadosTelemetria.length > 0) {
            dadosFormatados.timestamp = new Date();
            dadosTelemetria[0] = dadosFormatados;
            await persistirDados();
        } else {
            await salvarTelemetria(dadosFormatados);
        }
    }

    // --- EMISSÃO SOCKET.IO (TEMPO REAL) ---
    io.emit('telemetria-update', dadosFormatados);
    console.log('📡 Dados propagados via WebSocket.');

  } catch (erro) {
    console.error('❌ Erro msg:', erro.message);
  }
});

// --- SOCKET.IO (REALTIME) ---
io.on('connection', (socket) => {
  console.log(`🔌 Cliente Socket conectado: ${socket.id}`);

  if (socket.recovered) {
    console.log(`🔄 Sessão recuperada para: ${socket.id}`);
  }

  socket.on('disconnect', (reason) => {
    console.log(`🔌 Cliente Socket desconectado: ${socket.id} | Motivo: ${reason}`);
  });
});

// --- ROTAS API (HTTP) ---

app.get('/api/telemetria/latest', (req, res) => {
  const dados = dadosTelemetria[0];
  if (!dados) return res.status(404).json({ error: 'Sem dados.' });
  res.json(dados);
});

app.get('/api/telemetria/historico', (req, res) => {
  // Retorna apenas registros que são marcados como 'novo_evento' ou todos?
  // Vamos retornar todos os 10 primeiros do array (que agora contém 1 update recente + históricos passados)
  res.json(dadosTelemetria.slice(0, 10));
});

app.delete('/api/telemetria/historico', async (req, res) => {
  try {
    dadosTelemetria = [];
    await persistirDados();
    res.json({ message: 'Histórico limpo.' });
  } catch (error) {
    console.error('❌ Erro ao limpar histórico:', error.message);
    res.status(500).json({ error: 'Falha ao limpar histórico.' });
  }
});

app.get('/api/health', (req, res) => {
  res.json({ status: 'OK', mqtt: clienteMqtt.connected ? 'Online' : 'Offline' });
});

// Serve a página principal
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, '../index.html'));
});

// --- INICIALIZAÇÃO ---

async function iniciar() {
  await carregarDadosIniciais();
  server.listen(PORT, () => {
    console.log(`🚀 Servidor rodando em http://localhost:${PORT}`);
  });
}

iniciar();
