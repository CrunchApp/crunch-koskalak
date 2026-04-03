/**
 * Core type definitions for server-gadget
 */

export interface DeviceRegistration {
  deviceId: string;
  organizationId: string;
  locationId: string;
  name: string;
  firmwareVersion: string;
  registeredAt: Date;
  lastSeen: Date;
  status: DeviceStatus;
  config: DeviceConfig;
}

export type DeviceStatus = 'online' | 'offline' | 'updating' | 'error';

export interface DeviceConfig {
  voiceEnabled: boolean;
  wakeWord: string;
  personality: 'friendly' | 'professional' | 'playful';
  idleTimeoutMinutes: number;
  volume: number; /* 0-100 */
}

export interface DeviceJwtPayload {
  deviceId: string;
  organizationId: string;
  locationId: string;
  iat: number;
  exp: number;
}

export interface ProvisioningRequest {
  organizationId: string;
  locationId: string;
  oneTimeToken: string;
  firmwareVersion: string;
  macAddress: string;
}

export interface TelemetryReport {
  deviceId: string;
  batteryPercent: number;
  wifiSignalDbm: number;
  uptimeSeconds: number;
  freeHeapBytes: number;
  firmwareVersion: string;
  timestamp: Date;
}

export interface VoiceSessionEvent {
  type: 'session_start' | 'audio_chunk' | 'transcript' | 'response' | 'session_end';
  deviceId: string;
  sessionId: string;
  data?: Buffer | string;
  timestamp: Date;
}

export interface OtaManifest {
  version: string;
  url: string;
  sha256: string;
  size: number;
  releaseNotes: string;
  releasedAt: Date;
}

export interface CrunchEvent {
  type: string;
  organizationId: string;
  locationId?: string;
  payload: Record<string, unknown>;
  timestamp: Date;
}
