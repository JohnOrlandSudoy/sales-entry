import cors from 'cors';
import dotenv from 'dotenv';
import express from 'express';
import nodemailer from 'nodemailer';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
dotenv.config({ path: path.join(__dirname, '.env') });

const PORT = Number(process.env.PORT) || 3001;

function createTransport() {
  const host = process.env.SMTP_HOST;
  const user = process.env.SMTP_USER;
  const pass = process.env.SMTP_PASS;
  if (!host || !user || !pass) {
    return null;
  }
  return nodemailer.createTransport({
    host,
    port: Number(process.env.SMTP_PORT) || 587,
    secure: process.env.SMTP_SECURE === 'true',
    auth: { user, pass },
  });
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function buildHtml(body) {
  const { promoHeadName, rows } = body;
  const tr = (rows || [])
    .map(
      (r) =>
        `<tr>
          <td>${escapeHtml(r.date)}</td>
          <td>${escapeHtml(r.salesman)}</td>
          <td>${escapeHtml(r.barcode)}</td>
          <td style="text-align:right">${Number(r.price).toFixed(2)}</td>
          <td style="text-align:right">${r.qty}</td>
          <td style="text-align:right">${r.totQty ?? ''}</td>
          <td style="text-align:right">${r.sysTotal ?? ''}</td>
          <td style="text-align:right">${r.manual ?? ''}</td>
        </tr>`
    )
    .join('');

  return `<!DOCTYPE html>
<html><body style="font-family:Arial,sans-serif;background:#0f172a;color:#e2e8f0;padding:16px">
  <h2 style="color:#eab308">Daily Sales Invoice</h2>
  <p>Promo head: <strong>${escapeHtml(promoHeadName || '')}</strong></p>
  <table border="1" cellpadding="4" cellspacing="0" style="border-collapse:collapse;font-size:12px;width:100%">
    <thead style="background:#1e293b;color:#eab308">
      <tr>
        <th>Date</th><th>Salesman</th><th>Barcode</th>
        <th>Price</th><th>Qty</th><th>Tot Qty</th><th>Sys Total</th><th>Manual</th>
      </tr>
    </thead>
    <tbody>${tr}</tbody>
  </table>
</body></html>`;
}

const app = express();
app.use(cors());
app.use(express.json({ limit: '512kb' }));

app.get('/api/health', (_req, res) => {
  const transport = createTransport();
  res.json({
    ok: true,
    smtpConfigured: !!transport,
    port: PORT,
  });
});

app.post('/api/send-invoice', async (req, res) => {
  const transport = createTransport();
  if (!transport) {
    return res.status(503).json({
      ok: false,
      error: 'SMTP not configured. Copy server/.env.example to server/.env and set SMTP_* values.',
    });
  }

  const {
    to,
    from,
    promoHeadName,
    subject,
    text,
    html,
    rows,
  } = req.body || {};

  if (!to || !String(to).includes('@')) {
    return res.status(400).json({ ok: false, error: 'Missing or invalid "to" email address.' });
  }

  const mailFrom = from || process.env.SMTP_FROM || process.env.SMTP_USER;
  const mailSubject =
    subject || `Daily Sales Invoice — ${promoHeadName || 'Promo Head'}`;

  try {
    const info = await transport.sendMail({
      from: mailFrom,
      to: String(to).trim(),
      subject: mailSubject,
      text: text || 'See HTML invoice attached in body.',
      html: html || buildHtml({ promoHeadName, rows }),
    });
    res.json({ ok: true, messageId: info.messageId });
  } catch (err) {
    console.error('send-invoice error:', err);
    res.status(500).json({
      ok: false,
      error: err.message || 'Failed to send email',
    });
  }
});

app.listen(PORT, () => {
  const transport = createTransport();
  console.log(`Salesentry email server http://localhost:${PORT}`);
  console.log(transport ? 'SMTP: configured' : 'SMTP: NOT configured — create server/.env');
});
