import dotenv from 'dotenv';
dotenv.config();

export const config = {
  port: parseInt(process.env.PORT || '3004', 10),
  nodeEnv: process.env.NODE_ENV || 'development',

  crunch: {
    apiUrl: process.env.CRUNCH_API_URL || 'http://localhost:3000/api/v1',
    chatkitUrl: process.env.CRUNCH_CHATKIT_URL || 'http://localhost:8001',
    mcpUrl: process.env.CRUNCH_MCP_URL || 'http://localhost:8787',
  },

  openai: {
    apiKey: process.env.OPENAI_API_KEY || '',
  },

  device: {
    jwtSecret: process.env.DEVICE_JWT_SECRET || 'dev-secret-change-me',
  },

  mqtt: {
    port: parseInt(process.env.MQTT_PORT || '1883', 10),
    externalBrokerUrl: process.env.MQTT_BROKER_URL,
  },

  ota: {
    firmwarePath: process.env.OTA_FIRMWARE_PATH || './ota-binaries',
  },
} as const;
