/**
 * Crunch Gadget Gateway — Entry Point
 *
 * Lightweight service bridging ESP32-S3 devices to the Crunch platform.
 * Responsibilities:
 *   - Device registration and JWT-based auth
 *   - Voice proxy (audio ↔ OpenAI Whisper/TTS ↔ server-chatkit Sage agent)
 *   - Event fan-out (server-ops webhooks → MQTT → devices)
 *   - OTA firmware management
 *   - MQTT broker (embedded aedes for dev, external for production)
 *
 * Port: 3004 (HTTP + WebSocket)
 * MQTT: 1883 (embedded broker)
 */

import express from 'express';
import cors from 'cors';
import helmet from 'helmet';
import { createServer } from 'http';
import { WebSocketServer, WebSocket } from 'ws';
import { config } from './config';
import { deviceAuth } from './middleware/device-auth.middleware';
import * as voiceProxy from './services/voice-proxy.service';
import { setMqttPublisher } from './services/event-fanout.service';

/* Routes */
import deviceRoutes from './routes/device.routes';
import voiceRoutes from './routes/voice.routes';
import otaRoutes from './routes/ota.routes';
import webhookRoutes from './routes/webhook.routes';

const app = express();
const server = createServer(app);

/* Middleware */
app.use(helmet());
app.use(cors());
app.use(express.json());
app.use(express.raw({ type: 'application/octet-stream', limit: '5mb' }));

/* Health check */
app.get('/health', (_req, res) => {
  res.json({ status: 'ok', service: 'server-gadget', port: config.port });
});

/* API Routes */
app.use('/api/devices', deviceRoutes);
app.use('/api/voice', voiceRoutes);
app.use('/api/ota', otaRoutes);
app.use('/api/webhooks', webhookRoutes);

/* WebSocket server for voice sessions */
const wss = new WebSocketServer({ server, path: '/ws/voice' });

wss.on('connection', (ws: WebSocket, req) => {
  console.log('Voice session WebSocket connected');

  const audioChunks: Buffer[] = [];

  ws.on('message', async (data: Buffer | string) => {
    if (typeof data === 'string') {
      try {
        const msg = JSON.parse(data);
        if (msg.type === 'end_of_speech') {
          /* Process accumulated audio */
          const audioBuffer = Buffer.concat(audioChunks);
          audioChunks.length = 0;

          try {
            /* TODO: Extract orgId/locationId from WebSocket auth handshake */
            const result = await voiceProxy.processVoiceInput(
              audioBuffer,
              msg.organizationId || '',
              msg.locationId || '',
            );

            /* Send transcript */
            ws.send(JSON.stringify({
              type: 'transcript',
              text: result.transcript,
            }));

            /* Send response text (for display) */
            ws.send(JSON.stringify({
              type: 'response',
              text: result.textResponse,
            }));

            /* Send TTS audio as binary */
            ws.send(result.audioResponse);
          } catch (error) {
            console.error('Voice processing error:', error);
            ws.send(JSON.stringify({
              type: 'error',
              message: 'Voice processing failed',
            }));
          }
        }
      } catch {
        /* Not JSON — treat as binary audio */
        audioChunks.push(Buffer.from(data));
      }
    } else {
      /* Binary frame — audio data from mic */
      audioChunks.push(Buffer.from(data));
    }
  });

  ws.on('close', () => {
    console.log('Voice session ended');
    audioChunks.length = 0;
  });
});

/* TODO: Initialise MQTT broker (aedes for dev, external URL for production) */
/* For now, provide a stub publisher */
setMqttPublisher((topic: string, payload: string) => {
  console.log(`[MQTT stub] ${topic}: ${payload.slice(0, 100)}...`);
});

/* Start server */
server.listen(config.port, () => {
  console.log(`server-gadget running on port ${config.port}`);
  console.log(`  HTTP API: http://localhost:${config.port}/api`);
  console.log(`  WebSocket: ws://localhost:${config.port}/ws/voice`);
  console.log(`  Health: http://localhost:${config.port}/health`);
});

export { app, server };
