/**
 * Voice Proxy Service
 *
 * Bridges the gadget's audio stream to OpenAI APIs and server-chatkit.
 *
 * Flow:
 *   1. Receive PCM16 audio frames from gadget via WebSocket
 *   2. Buffer audio until end-of-speech signal
 *   3. Send to OpenAI Whisper API for transcription
 *   4. Forward transcript to server-chatkit (Sage agent)
 *   5. Receive response text from Sage
 *   6. Send to OpenAI TTS API
 *   7. Stream TTS audio back to gadget via WebSocket
 *
 * Upgrade path: Replace steps 2-6 with OpenAI Realtime API for
 * streaming voice-to-voice with lower latency.
 */

import OpenAI from 'openai';
import axios from 'axios';
import { config } from '../config';

const openai = new OpenAI({ apiKey: config.openai.apiKey });

/**
 * Transcribe audio buffer using OpenAI Whisper API.
 */
export async function transcribeAudio(audioBuffer: Buffer): Promise<string> {
  const file = new File([audioBuffer], 'audio.wav', { type: 'audio/wav' });

  const transcription = await openai.audio.transcriptions.create({
    file,
    model: 'whisper-1',
    language: 'en',
    response_format: 'text',
  });

  return transcription as unknown as string;
}

/**
 * Send a transcript to the Sage agent via server-chatkit and get a text response.
 *
 * TODO: Implement proper agent session management with conversation context.
 *       For now, each request is stateless.
 */
export async function getAgentResponse(
  transcript: string,
  organizationId: string,
  locationId: string,
): Promise<string> {
  try {
    const response = await axios.post(`${config.crunch.chatkitUrl}/api/chat`, {
      message: transcript,
      agentType: 'gadget',
      context: {
        organizationId,
        locationId,
        source: 'gadget-voice',
      },
    });

    return response.data.response || "Sorry, I didn't catch that. Could you try again?";
  } catch (error) {
    console.error('Agent response error:', error);
    return "I'm having a bit of trouble right now. Let me get a team member to help you.";
  }
}

/**
 * Convert text to speech using OpenAI TTS API.
 * Returns raw audio buffer suitable for streaming to the gadget.
 */
export async function textToSpeech(text: string): Promise<Buffer> {
  const mp3Response = await openai.audio.speech.create({
    model: 'tts-1',
    voice: 'nova',
    input: text,
    response_format: 'pcm',   /* Raw PCM16 — avoids decoding on ESP32 */
    speed: 1.0,
  });

  const arrayBuffer = await mp3Response.arrayBuffer();
  return Buffer.from(arrayBuffer);
}

/**
 * Full voice pipeline: audio in → transcript → agent response → audio out.
 * Returns the TTS audio buffer and the text response for display.
 */
export async function processVoiceInput(
  audioBuffer: Buffer,
  organizationId: string,
  locationId: string,
): Promise<{ audioResponse: Buffer; textResponse: string; transcript: string }> {
  const transcript = await transcribeAudio(audioBuffer);
  const textResponse = await getAgentResponse(transcript, organizationId, locationId);
  const audioResponse = await textToSpeech(textResponse);

  return { audioResponse, textResponse, transcript };
}
