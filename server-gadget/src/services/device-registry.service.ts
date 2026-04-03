/**
 * Device Registry Service
 *
 * Manages device registration, provisioning, and status tracking.
 * In production, this would be backed by a database. For now, in-memory store.
 *
 * TODO: Replace in-memory Map with MongoDB collection (shared with server-ops)
 *       or a dedicated device registry table.
 */

import jwt from 'jsonwebtoken';
import { v4 as uuidv4 } from 'uuid';
import { config } from '../config';
import {
  DeviceRegistration,
  DeviceJwtPayload,
  ProvisioningRequest,
  TelemetryReport,
} from '../types';

/* In-memory device store (replace with DB in production) */
const devices = new Map<string, DeviceRegistration>();
const pendingTokens = new Map<string, { organizationId: string; locationId: string; expiresAt: Date }>();

/**
 * Generate a one-time provisioning token for a new device.
 * Called from the web-platform dashboard when an operator adds a device.
 */
export function createProvisioningToken(organizationId: string, locationId: string): string {
  const token = uuidv4();
  pendingTokens.set(token, {
    organizationId,
    locationId,
    expiresAt: new Date(Date.now() + 15 * 60 * 1000), /* 15 minute expiry */
  });
  return token;
}

/**
 * Exchange a one-time provisioning token for a device JWT.
 * Called by the gadget during initial setup.
 */
export function provisionDevice(request: ProvisioningRequest): { deviceId: string; jwt: string } | null {
  const pending = pendingTokens.get(request.oneTimeToken);
  if (!pending || pending.expiresAt < new Date()) {
    pendingTokens.delete(request.oneTimeToken);
    return null;
  }

  if (pending.organizationId !== request.organizationId || pending.locationId !== request.locationId) {
    return null;
  }

  pendingTokens.delete(request.oneTimeToken);

  const deviceId = uuidv4();
  const device: DeviceRegistration = {
    deviceId,
    organizationId: request.organizationId,
    locationId: request.locationId,
    name: `Gadget ${deviceId.slice(0, 8)}`,
    firmwareVersion: request.firmwareVersion,
    registeredAt: new Date(),
    lastSeen: new Date(),
    status: 'online',
    config: {
      voiceEnabled: true,
      wakeWord: 'hey-crunchy',
      personality: 'friendly',
      idleTimeoutMinutes: 5,
      volume: 70,
    },
  };

  devices.set(deviceId, device);

  const payload: DeviceJwtPayload = {
    deviceId,
    organizationId: request.organizationId,
    locationId: request.locationId,
    iat: Math.floor(Date.now() / 1000),
    exp: Math.floor(Date.now() / 1000) + 365 * 24 * 60 * 60, /* 1 year */
  };

  const token = jwt.sign(payload, config.device.jwtSecret);
  return { deviceId, jwt: token };
}

/**
 * Update device telemetry and last-seen timestamp.
 */
export function updateTelemetry(report: TelemetryReport): void {
  const device = devices.get(report.deviceId);
  if (device) {
    device.lastSeen = new Date();
    device.status = 'online';
    device.firmwareVersion = report.firmwareVersion;
  }
}

/**
 * Get a device by ID.
 */
export function getDevice(deviceId: string): DeviceRegistration | undefined {
  return devices.get(deviceId);
}

/**
 * List all devices for an organization, optionally filtered by location.
 */
export function listDevices(organizationId: string, locationId?: string): DeviceRegistration[] {
  return Array.from(devices.values()).filter(d => {
    if (d.organizationId !== organizationId) return false;
    if (locationId && d.locationId !== locationId) return false;
    return true;
  });
}
