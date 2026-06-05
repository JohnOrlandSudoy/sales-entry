import type { AppSettings, SalesEntry } from './types';

export interface InvoiceRow {
  date: string;
  salesman: string;
  barcode: string;
  price: number;
  qty: number;
  totQty: string;
  sysTotal: string;
  manual: string;
}

/** Same row order as promo review table in App.tsx */
export function buildInvoiceRows(entries: SalesEntry[]): InvoiceRow[] {
  const flat = entries.flatMap((entry) => {
    const lastIdx = entry.items.length - 1;
    return entry.items.map((item, itemIdx) => ({
      entry,
      item,
      itemIdx,
      isLastInSale: itemIdx === lastIdx,
    }));
  });

  flat.sort((a, b) => {
    const byName = a.entry.employeeName.localeCompare(b.entry.employeeName);
    if (byName !== 0) return byName;
    return a.entry.date.localeCompare(b.entry.date);
  });

  const totalQtyBySalesman = new Map<string, number>();
  for (const { entry, item } of flat) {
    const name = entry.employeeName;
    totalQtyBySalesman.set(name, (totalQtyBySalesman.get(name) ?? 0) + item.quantity);
  }

  const lastRowBySalesman = new Map<string, number>();
  flat.forEach((r, i) => lastRowBySalesman.set(r.entry.employeeName, i));

  return flat.map((r, i) => {
    const isLastForSalesman = lastRowBySalesman.get(r.entry.employeeName) === i;
    const salesmanTotalQty = totalQtyBySalesman.get(r.entry.employeeName) ?? 0;
    return {
      date: r.entry.date,
      salesman: r.entry.employeeName,
      barcode: r.item.barcode,
      price: r.item.price,
      qty: r.item.quantity,
      totQty: isLastForSalesman ? String(salesmanTotalQty) : '',
      sysTotal: r.isLastInSale ? r.entry.grandTotal.toFixed(2) : '',
      manual: r.isLastInSale ? (r.entry.manualTotal ?? 0).toFixed(2) : '',
    };
  });
}

export function buildInvoicePlainText(
  settings: AppSettings,
  entries: SalesEntry[]
): string {
  const rows = buildInvoiceRows(entries);
  const lines = [
    `Daily Sales Invoice`,
    `Promo head: ${settings.promoHeadName}`,
    '',
    'Date\tSalesman\tBarcode\tPrice\tQty\tTotQty\tSysTotal\tManual',
    ...rows.map(
      (r) =>
        `${r.date}\t${r.salesman}\t${r.barcode}\t${r.price}\t${r.qty}\t${r.totQty}\t${r.sysTotal}\t${r.manual}`
    ),
  ];
  return lines.join('\n');
}

export async function sendInvoiceEmail(
  settings: AppSettings,
  entries: SalesEntry[]
): Promise<{ ok: true } | { ok: false; error: string }> {
  const to = settings.recipientEmail?.trim();
  if (!to) {
    return { ok: false, error: 'Set "Invoice recipient email" in Settings first.' };
  }

  const rows = buildInvoiceRows(entries);
  const promoHeadName = settings.promoHeadName;
  const subject = `Daily Sales Invoice — ${promoHeadName}`;
  const text = buildInvoicePlainText(settings, entries);

  const res = await fetch('/api/send-invoice', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      to,
      from: settings.senderEmail?.trim() || undefined,
      promoHeadName,
      subject,
      text,
      rows,
    }),
  });

  const data = (await res.json().catch(() => ({}))) as {
    ok?: boolean;
    error?: string;
  };

  if (!res.ok || !data.ok) {
    return {
      ok: false,
      error: data.error || `Email server error (${res.status})`,
    };
  }

  return { ok: true };
}

export async function checkEmailServer(): Promise<boolean> {
  try {
    const res = await fetch('/api/health');
    if (!res.ok) return false;
    const data = (await res.json()) as { smtpConfigured?: boolean };
    return !!data.smtpConfigured;
  } catch {
    return false;
  }
}
