/**
 * Event Fan-out Service
 *
 * Receives webhook events from server-ops and routes them to
 * the appropriate devices via MQTT topics.
 *
 * server-ops emits webhooks → server-gadget receives them →
 * filters by location → publishes to device MQTT topics.
 *
 * Subscribed events:
 *   - sales_order.created / updated / completed
 *   - inventory.low_stock / out_of_stock
 *   - item.updated (menu changes, 86'd items)
 */

import { CrunchEvent } from '../types';

type MqttPublishFn = (topic: string, payload: string) => void;

let mqttPublish: MqttPublishFn | null = null;

/**
 * Set the MQTT publish function (injected from index.ts after broker init).
 */
export function setMqttPublisher(publishFn: MqttPublishFn): void {
  mqttPublish = publishFn;
}

/**
 * Handle an incoming webhook event from server-ops.
 * Routes to all devices at the matching location via MQTT.
 */
export function handleWebhookEvent(event: CrunchEvent): void {
  if (!mqttPublish) {
    console.warn('MQTT publisher not initialised, dropping event:', event.type);
    return;
  }

  const { organizationId, locationId, type, payload } = event;

  if (!locationId) {
    /* Org-wide event — broadcast to all devices in the org */
    const topic = `crunch/${organizationId}/+/+/events`;
    mqttPublish(topic, JSON.stringify({ type, payload, timestamp: event.timestamp }));
    return;
  }

  /* Location-specific event — send to all devices at this location */
  const topic = `crunch/${organizationId}/${locationId}/+/events`;
  mqttPublish(topic, JSON.stringify({ type, payload, timestamp: event.timestamp }));
}

/**
 * Filter function: determines if a device should receive a given event.
 * Used for more granular filtering beyond location matching.
 */
export function shouldDeviceReceiveEvent(event: CrunchEvent, _deviceId: string): boolean {
  /* For MVP, all devices at a location receive all events for that location.
   * TODO: Add role-based filtering (e.g., host-stand devices get order events,
   *       table devices only get their own table's events).
   */
  const relevantEvents = [
    'sales_order.created',
    'sales_order.updated',
    'sales_order.completed',
    'inventory.low_stock',
    'inventory.out_of_stock',
    'item.updated',
    'item.status_changed',
  ];

  return relevantEvents.includes(event.type);
}
