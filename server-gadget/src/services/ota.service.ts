/**
 * OTA (Over-the-Air) Update Service
 *
 * Manages firmware versions and serves update binaries to devices.
 * The ESP32-S3 uses dual A/B partitions for safe OTA updates:
 *   - Device boots from partition A
 *   - New firmware written to partition B
 *   - On successful boot from B, B becomes the active partition
 *   - If B fails to boot, watchdog reverts to A automatically
 *
 * Update flow:
 *   1. New firmware binary uploaded to server-gadget
 *   2. Dashboard triggers update for specific device(s) or fleet-wide
 *   3. server-gadget publishes OTA command to device's MQTT topic
 *   4. Device downloads firmware binary via HTTPS from server-gadget
 *   5. Device writes to inactive partition, reboots
 *   6. Device reports new version in next telemetry heartbeat
 */

import { readdir, stat, readFile } from 'fs/promises';
import { join } from 'path';
import { createHash } from 'crypto';
import { config } from '../config';
import { OtaManifest } from '../types';

/**
 * List available firmware versions in the OTA directory.
 */
export async function listFirmwareVersions(): Promise<OtaManifest[]> {
  const dir = config.ota.firmwarePath;
  const manifests: OtaManifest[] = [];

  try {
    const files = await readdir(dir);
    for (const file of files) {
      if (!file.endsWith('.bin')) continue;

      const filePath = join(dir, file);
      const fileStat = await stat(filePath);
      const content = await readFile(filePath);
      const sha256 = createHash('sha256').update(content).digest('hex');

      /* Extract version from filename: koskalak-v1.2.3.bin */
      const versionMatch = file.match(/v(\d+\.\d+\.\d+)/);
      const version = versionMatch ? versionMatch[1] : file;

      manifests.push({
        version,
        url: `/api/ota/firmware/${file}`,
        sha256,
        size: fileStat.size,
        releaseNotes: '',
        releasedAt: fileStat.mtime,
      });
    }
  } catch {
    /* OTA directory may not exist yet */
  }

  return manifests.sort((a, b) => b.version.localeCompare(a.version, undefined, { numeric: true }));
}

/**
 * Get the latest firmware version available.
 */
export async function getLatestVersion(): Promise<OtaManifest | null> {
  const versions = await listFirmwareVersions();
  return versions[0] || null;
}

/**
 * Check if a device needs an update.
 */
export async function deviceNeedsUpdate(currentVersion: string): Promise<OtaManifest | null> {
  const latest = await getLatestVersion();
  if (!latest) return null;

  if (latest.version !== currentVersion) {
    return latest;
  }
  return null;
}
