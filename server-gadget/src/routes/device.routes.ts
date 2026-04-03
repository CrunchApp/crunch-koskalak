/**
 * Device Routes
 *
 * POST /api/devices/provision   — Exchange one-time token for device JWT
 * GET  /api/devices             — List devices (dashboard use)
 * GET  /api/devices/:id         — Get device details
 * POST /api/devices/:id/config  — Update device configuration
 */

import { Router, Request, Response } from 'express';
import { deviceAuth } from '../middleware/device-auth.middleware';
import * as registry from '../services/device-registry.service';

const router = Router();

/**
 * Device provisioning — no auth required (uses one-time token).
 */
router.post('/provision', (req: Request, res: Response) => {
  const { organizationId, locationId, oneTimeToken, firmwareVersion, macAddress } = req.body;

  if (!organizationId || !locationId || !oneTimeToken) {
    res.status(400).json({ error: 'Missing required fields' });
    return;
  }

  const result = registry.provisionDevice({
    organizationId,
    locationId,
    oneTimeToken,
    firmwareVersion: firmwareVersion || '0.0.0',
    macAddress: macAddress || 'unknown',
  });

  if (!result) {
    res.status(401).json({ error: 'Invalid or expired provisioning token' });
    return;
  }

  res.json({
    deviceId: result.deviceId,
    token: result.jwt,
    mqtt: {
      topic: `crunch/${organizationId}/${locationId}/${result.deviceId}`,
    },
  });
});

/**
 * List devices — requires device or dashboard auth.
 */
router.get('/', deviceAuth, (req: Request, res: Response) => {
  const devices = registry.listDevices(
    req.device!.organizationId,
    req.query.locationId as string | undefined,
  );
  res.json({ devices });
});

/**
 * Get device by ID.
 */
router.get('/:id', deviceAuth, (req: Request, res: Response) => {
  const device = registry.getDevice(req.params.id);
  if (!device || device.organizationId !== req.device!.organizationId) {
    res.status(404).json({ error: 'Device not found' });
    return;
  }
  res.json({ device });
});

export default router;
