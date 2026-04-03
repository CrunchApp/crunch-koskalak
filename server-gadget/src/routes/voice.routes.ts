/**
 * Voice Routes
 *
 * WebSocket endpoint for voice sessions.
 * HTTP endpoint for one-shot voice processing (fallback).
 *
 * WebSocket protocol:
 *   Client → Server: Binary frames (PCM16 audio from mic)
 *   Server → Client: Binary frames (PCM16 TTS audio)
 *   Client → Server: JSON frames ({ type: 'end_of_speech' })
 *   Server → Client: JSON frames ({ type: 'transcript', text: '...' })
 *   Server → Client: JSON frames ({ type: 'response', text: '...' })
 */

import { Router, Request, Response } from 'express';
import { deviceAuth } from '../middleware/device-auth.middleware';
import * as voiceProxy from '../services/voice-proxy.service';

const router = Router();

/**
 * One-shot voice processing (HTTP fallback for devices that can't hold WebSocket).
 * Accepts audio as multipart/form-data, returns JSON with transcript + TTS audio URL.
 */
router.post('/process', deviceAuth, async (req: Request, res: Response) => {
  try {
    if (!req.body || !Buffer.isBuffer(req.body)) {
      res.status(400).json({ error: 'Expected raw audio body' });
      return;
    }

    const result = await voiceProxy.processVoiceInput(
      req.body,
      req.device!.organizationId,
      req.device!.locationId,
    );

    res.json({
      transcript: result.transcript,
      response: result.textResponse,
      /* Audio returned as base64 for HTTP fallback */
      audio: result.audioResponse.toString('base64'),
    });
  } catch (error) {
    console.error('Voice processing error:', error);
    res.status(500).json({ error: 'Voice processing failed' });
  }
});

export default router;
