/**
 * Webhook Routes
 *
 * POST /api/webhooks/crunch — Receive webhook events from server-ops
 *
 * server-ops sends webhook events when orders, inventory, or items change.
 * This endpoint validates the HMAC signature and routes events to devices
 * via the event fan-out service.
 */

import { Router, Request, Response } from 'express';
import crypto from 'crypto';
import { handleWebhookEvent } from '../services/event-fanout.service';
import { CrunchEvent } from '../types';

const router = Router();

/**
 * Validate HMAC-SHA256 signature from server-ops webhook delivery.
 */
function verifyWebhookSignature(payload: string, signature: string, secret: string): boolean {
  const expected = crypto.createHmac('sha256', secret).update(payload).digest('hex');
  return crypto.timingSafeEqual(Buffer.from(signature), Buffer.from(expected));
}

router.post('/crunch', (req: Request, res: Response) => {
  const signature = req.headers['x-crunch-signature'] as string;
  const webhookSecret = process.env.CRUNCH_WEBHOOK_SECRET;

  if (webhookSecret && signature) {
    const rawBody = JSON.stringify(req.body);
    if (!verifyWebhookSignature(rawBody, signature, webhookSecret)) {
      res.status(401).json({ error: 'Invalid webhook signature' });
      return;
    }
  }

  const event: CrunchEvent = {
    type: req.body.event,
    organizationId: req.body.organizationId,
    locationId: req.body.locationId,
    payload: req.body.data || {},
    timestamp: new Date(req.body.timestamp || Date.now()),
  };

  handleWebhookEvent(event);
  res.status(200).json({ received: true });
});

export default router;
