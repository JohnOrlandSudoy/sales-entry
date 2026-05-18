import type { AppSettings, SalesEntry, CashReconciliation, WeekRecord } from './types';

const SETTINGS_KEY = 'dse_settings_v2'; // Updated key to force default reset
const SALES_KEY = 'dse_sales';
const SENT_SALES_KEY = 'dse_sent_sales';
const RECON_KEY = 'dse_reconciliation';

const DEFAULT_SETTINGS: AppSettings = {
  systemPin: '1234',
  masterPin: '9999',
  employees: [
    { id: '1', name: 'John' },
    { id: '2', name: 'Howard lee' },
  ],
  sentDates: [],
  sentSalesKeys: [],
  promoHeadName: 'Ms. Helen',
  senderEmail: '',
};

export function loadSettings(): AppSettings {
  const raw = localStorage.getItem(SETTINGS_KEY);
  if (!raw) {
    saveSettings(DEFAULT_SETTINGS);
    return DEFAULT_SETTINGS;
  }
  const parsed = JSON.parse(raw) as Partial<AppSettings>;
  return {
    ...DEFAULT_SETTINGS,
    ...parsed,
    employees: parsed.employees ?? DEFAULT_SETTINGS.employees,
    sentDates: parsed.sentDates ?? DEFAULT_SETTINGS.sentDates,
    sentSalesKeys: parsed.sentSalesKeys ?? DEFAULT_SETTINGS.sentSalesKeys,
    promoHeadName: parsed.promoHeadName?.trim() || DEFAULT_SETTINGS.promoHeadName,
    senderEmail: parsed.senderEmail?.trim() ?? DEFAULT_SETTINGS.senderEmail,
  };
}

export function saveSettings(s: AppSettings) {
  localStorage.setItem(SETTINGS_KEY, JSON.stringify(s));
}

export function loadSales(): SalesEntry[] {
  const raw = localStorage.getItem(SALES_KEY);
  return raw ? JSON.parse(raw) : [];
}

export function saveSales(entries: SalesEntry[]) {
  localStorage.setItem(SALES_KEY, JSON.stringify(entries));
}

export function loadSentSales(): SalesEntry[] {
  const raw = localStorage.getItem(SENT_SALES_KEY);
  return raw ? JSON.parse(raw) : [];
}

export function saveSentSales(entries: SalesEntry[]) {
  localStorage.setItem(SENT_SALES_KEY, JSON.stringify(entries));
}

export function loadReconciliation(): CashReconciliation[] {
  const raw = localStorage.getItem(RECON_KEY);
  return raw ? JSON.parse(raw) : [];
}

export function saveReconciliation(recs: CashReconciliation[]) {
  localStorage.setItem(RECON_KEY, JSON.stringify(recs));
}

export function getWeekKey(date: Date): string {
  const start = new Date(date);
  const day = start.getDay();
  const diff = start.getDate() - day + (day === 0 ? -6 : 1);
  const monday = new Date(start.setDate(diff));
  const y = monday.getFullYear();
  const m = String(monday.getMonth() + 1).padStart(2, '0');
  const d = String(monday.getDate()).padStart(2, '0');
  return `${y}-${m}-${d}`;
}

export function getWeekLabel(weekKey: string): string {
  return `Week of ${weekKey}`;
}

export function buildWeekRecords(entries: SalesEntry[]): WeekRecord[] {
  const map = new Map<string, SalesEntry[]>();
  for (const e of entries) {
    if (!map.has(e.weekKey)) map.set(e.weekKey, []);
    map.get(e.weekKey)!.push(e);
  }
  const records: WeekRecord[] = [];
  for (const [weekKey, weekEntries] of map) {
    records.push({
      weekKey,
      weekLabel: getWeekLabel(weekKey),
      entries: weekEntries,
    });
  }
  records.sort((a, b) => b.weekKey.localeCompare(a.weekKey));
  return records;
}

export function salesSentKey(employeeId: string, date: string): string {
  return `${employeeId}|${date}`;
}

export function isEntryReconciled(entry: SalesEntry, recs: CashReconciliation[]): boolean {
  return recs.some(
    (r) =>
      r.employeeId === entry.employeeId &&
      r.status === 'MATCH' &&
      Math.abs(r.systemTotal - entry.grandTotal) < 0.01
  );
}

export function genId(): string {
  return Date.now().toString(36) + Math.random().toString(36).slice(2, 7);
}

export function padBarcode(input: string): string {
  const digits = input.replace(/\D/g, '');
  return digits.padStart(13, '0');
}

export function generateSalesCSV(entries: SalesEntry[]): string {
  const header = 'Date,Salesman,Barcode,Price,Discount,Total\n';
  const rows = entries.flatMap(entry => 
    entry.items.map(item => 
      `${entry.date},"${entry.employeeName}",${item.barcode},${item.price},${item.discount},${item.lineTotal}`
    )
  ).join('\n');
  return header + rows;
}
