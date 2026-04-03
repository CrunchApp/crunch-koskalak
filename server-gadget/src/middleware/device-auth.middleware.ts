/**
 * Device Authentication Middleware
 *
 * Validates device JWTs on incoming requests.
 * JWTs are issued during provisioning and scoped to orgId + locationId.
 */

import { Request, Response, NextFunction } from 'express';
import jwt from 'jsonwebtoken';
import { config } from '../config';
import { DeviceJwtPayload } from '../types';

declare global {
  namespace Express {
    interface Request {
      device?: DeviceJwtPayload;
    }
  }
}

export function deviceAuth(req: Request, res: Response, next: NextFunction): void {
  const authHeader = req.headers.authorization;
  if (!authHeader?.startsWith('Bearer ')) {
    res.status(401).json({ error: 'Missing device token' });
    return;
  }

  const token = authHeader.slice(7);

  try {
    const payload = jwt.verify(token, config.device.jwtSecret) as DeviceJwtPayload;
    req.device = payload;
    next();
  } catch (err) {
    res.status(401).json({ error: 'Invalid or expired device token' });
  }
}
