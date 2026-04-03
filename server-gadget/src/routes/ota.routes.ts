/**
 * OTA (Over-the-Air Update) Routes
 *
 * GET  /api/ota/check          — Check if update is available
 * GET  /api/ota/firmware/:file — Download firmware binary
 * GET  /api/ota/versions       — List all available firmware versions
 */

import { Router, Request, Response } from 'express';
import { join } from 'path';
import { deviceAuth } from '../middleware/device-auth.middleware';
import * as ota from '../services/ota.service';
import { config } from '../config';

const router = Router();

/**
 * Check if a firmware update is available for this device.
 */
router.get('/check', deviceAuth, async (req: Request, res: Response) => {
  const currentVersion = req.query.version as string;
  if (!currentVersion) {
    res.status(400).json({ error: 'Missing version query parameter' });
    return;
  }

  const update = await ota.deviceNeedsUpdate(currentVersion);
  if (update) {
    res.json({ updateAvailable: true, ...update });
  } else {
    res.json({ updateAvailable: false });
  }
});

/**
 * Download a firmware binary.
 */
router.get('/firmware/:filename', deviceAuth, (req: Request, res: Response) => {
  const filePath = join(config.ota.firmwarePath, req.params.filename);
  res.sendFile(filePath, (err) => {
    if (err) {
      res.status(404).json({ error: 'Firmware not found' });
    }
  });
});

/**
 * List all available firmware versions.
 */
router.get('/versions', deviceAuth, async (_req: Request, res: Response) => {
  const versions = await ota.listFirmwareVersions();
  res.json({ versions });
});

export default router;
