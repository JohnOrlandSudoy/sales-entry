export interface Employee {
  id: string;
  name: string;
}

export interface SalesItem {
  id: string;
  barcode: string;
  price: number;
  discount: number;
  quantity: number;
  lineTotal: number;
}

export interface SalesEntry {
  id: string;
  employeeId: string;
  employeeName: string;
  items: SalesItem[];
  grandTotal: number;
  manualTotal?: number;
  status: 'OPEN' | 'CLOSED';
  date: string;
  weekKey: string;
}

export interface CashReconciliation {
  employeeId: string;
  systemTotal: number;
  actualCash: number;
  status: 'MATCH' | 'NOT MATCH';
}

export interface AppSettings {
  systemPin: string;
  masterPin: string;
  employees: Employee[];
  sentDates?: string[];
  promoHeadName: string;
  senderEmail: string;
}

export interface WeekRecord {
  weekKey: string;
  weekLabel: string;
  entries: SalesEntry[];
}

export type Screen =
  | 'lock'
  | 'sales'
  | 'dashboard'
  | 'settings'
  | 'history'
  | 'master-pin-challenge';
