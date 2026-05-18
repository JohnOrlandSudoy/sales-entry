import { useState } from 'react';
import type { SalesEntry, CashReconciliation, Employee } from '../types';
import { salesSentKey } from '../storage';
import NumericKeypad from './NumericKeypad';
import { CheckCircle, XCircle, List } from 'lucide-react';

interface Props {
  entries: SalesEntry[];
  employees: Employee[];
  reconciliation: CashReconciliation[];
  onReconcile: (rec: CashReconciliation) => void;
  onGoToSales: (employeeId: string) => void;
  selectedDate: string;
  isSent: boolean;
  sentSalesKeys: string[];
}

export default function Dashboard({
  entries,
  employees,
  reconciliation,
  onReconcile,
  onGoToSales,
  selectedDate,
  isSent,
  sentSalesKeys,
}: Props) {
  const sentKeySet = new Set(sentSalesKeys);
  const [cashInput, setCashInput] = useState('');
  const [selectedEmp, setSelectedEmp] = useState('');
  const [showKeypad, setShowKeypad] = useState(false);
  const [viewItemsEmpId, setViewItemsEmpId] = useState<string | null>(null);

  const todayEntries = entries.filter((e) => e.date === selectedDate);

  const summary = employees.map((emp) => {
    const empEntries = todayEntries.filter((e) => e.employeeId === emp.id);
    const totalSales = empEntries.reduce((s, e) => s + e.grandTotal, 0);
    const itemCount = empEntries.reduce((s, e) => s + e.items.length, 0);
    const hasOpen = empEntries.some((e) => e.status === 'OPEN');
    const allClosed = empEntries.length > 0 && !hasOpen;
    const isEmailed = sentKeySet.has(salesSentKey(emp.id, selectedDate));
    const status: 'OPEN' | 'CLOSED' | 'SENT' | '-' = isEmailed
      ? 'SENT'
      : empEntries.length === 0
        ? '-'
        : hasOpen
          ? 'OPEN'
          : 'CLOSED';
    const rec = reconciliation.find(
      (r) => r.employeeId === emp.id && Math.abs(r.systemTotal - totalSales) < 0.01
    );
    return { ...emp, totalSales, itemCount, status, allClosed, rec, entries: empEntries };
  });

  const selectedViewEmp = summary.find(s => s.id === viewItemsEmpId);

  const handleReconcile = () => {
    if (isSent) return;
    if (!selectedEmp || !cashInput) return;
    const emp = summary.find((s) => s.id === selectedEmp);
    if (!emp) return;
    const actualCash = parseFloat(cashInput) || 0;
    onReconcile({
      employeeId: selectedEmp,
      systemTotal: emp.totalSales,
      actualCash,
      status: Math.abs(actualCash - emp.totalSales) < 0.01 ? 'MATCH' : 'NOT MATCH',
    });
    setCashInput('');
    setSelectedEmp('');
    setShowKeypad(false);
  };

  return (
    <div className="w-[480px] h-[288px] bg-gray-950 flex select-none overflow-hidden">
      {/* Left: Summary table */}
      <div className="flex-1 flex flex-col min-w-0 p-1.5 gap-1">
        <div className="text-[10px] text-yellow-400 font-bold uppercase tracking-wider flex items-center justify-between">
          <div className="flex items-center gap-1">
            <span className="text-[12px]">₱</span> Daily Summary - {selectedDate}
          </div>
          {isSent && (
            <div className="flex items-center gap-1 bg-green-900/40 text-green-400 px-1.5 py-0.5 rounded border border-green-800 animate-pulse">
              <CheckCircle size={10} />
              <span className="text-[8px] font-black tracking-widest">SENT</span>
            </div>
          )}
        </div>
        <div className="flex-1 overflow-y-auto min-h-0 border border-gray-800 rounded bg-gray-900/50">
          <table className="w-full text-[9px]">
            <thead className="sticky top-0 bg-gray-900">
              <tr className="text-gray-500 uppercase">
                <th className="px-1 text-left">Name</th>
                <th className="px-1 text-right">Sales</th>
                <th className="px-1 text-right">Items</th>
                <th className="px-1 text-center">Status</th>
                <th className="px-1 text-center">Rec</th>
                <th className="px-1 w-6"></th>
              </tr>
            </thead>
            <tbody>
              {summary.map((s) => (
                <tr key={s.id} className="border-t border-gray-800/50">
                  <td className="px-1 text-green-400 font-semibold">{s.name}</td>
                  <td className="px-1 text-right text-yellow-400 font-mono">₱{s.totalSales.toFixed(2)}</td>
                  <td className="px-1 text-right text-gray-300 font-mono">{s.itemCount}</td>
                  <td className="px-1 text-center">
                    <span
                      className={`px-1 py-0.5 rounded text-[8px] font-bold ${
                        s.status === 'OPEN'
                          ? 'bg-orange-900/60 text-orange-400'
                          : s.status === 'CLOSED'
                            ? 'bg-green-900/60 text-green-400'
                            : s.status === 'SENT'
                              ? 'bg-cyan-900/60 text-cyan-400'
                              : 'text-gray-600'
                      }`}
                    >
                      {s.status}
                    </span>
                  </td>
                  <td className="px-1 text-center">
                    {s.rec ? (
                      <div className="flex items-center justify-center gap-1">
                        {s.rec.status === 'MATCH' ? (
                          <CheckCircle size={10} className="text-green-400" />
                        ) : (
                          <>
                            <XCircle size={10} className="text-red-400" />
                            <button
                              onClick={() => {
                                setSelectedEmp(s.id);
                                setShowKeypad(true);
                                setCashInput('');
                              }}
                              className="text-[7px] bg-red-900/40 text-red-300 px-1 rounded border border-red-800 hover:bg-red-800 transition-colors"
                            >
                              RETRY
                            </button>
                          </>
                        )}
                      </div>
                    ) : (
                      <span className="text-gray-700">-</span>
                    )}
                  </td>
                  <td className="px-1 w-10">
                    <div className="flex items-center gap-1">
                      {s.itemCount > 0 && (
                        <button
                          onClick={() => setViewItemsEmpId(s.id)}
                          className="text-yellow-500 hover:text-yellow-400"
                        >
                          <List size={10} />
                        </button>
                      )}
                      {(s.status === 'OPEN' || s.status === 'CLOSED') && (
                        <button
                          onClick={() => onGoToSales(s.id)}
                          className="text-cyan-400 hover:text-cyan-300 text-[8px] underline"
                        >
                          {isSent ? 'View' : s.status === 'CLOSED' ? 'View/Edit' : 'Edit'}
                        </button>
                      )}
                      {s.status === 'SENT' && (
                        <span className="text-[8px] text-cyan-500 font-bold">Emailed</span>
                      )}
                      {s.status === '-' && !isSent && (
                        <button
                          onClick={() => onGoToSales(s.id)}
                          className="text-green-500 hover:text-green-400 text-[8px] font-bold border border-green-900 px-1 rounded bg-green-900/20"
                        >
                          + ADD
                        </button>
                      )}
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>

        {/* Reconciliation section */}
        <div className="border-t border-gray-800 pt-1">
          <div className="text-[9px] text-gray-500 uppercase tracking-wider mb-0.5">Cash Reconciliation</div>
          <div className="flex items-center gap-1">
            <select
              value={selectedEmp}
              onChange={(e) => setSelectedEmp(e.target.value)}
              disabled={isSent}
              className="bg-gray-800 border border-gray-700 rounded px-1 py-0.5 text-[10px] text-green-400 outline-none flex-1 disabled:opacity-40 disabled:cursor-not-allowed"
            >
              <option value="">Select...</option>
              {summary
                .filter((s) => s.allClosed && (!s.rec || s.rec.status === 'NOT MATCH'))
                .map((s) => (
                  <option key={s.id} value={s.id}>
                    {s.name} (₱{s.totalSales.toFixed(2)})
                  </option>
                ))}
            </select>
            <button
              onClick={() => setShowKeypad(!showKeypad)}
              disabled={isSent}
              className="h-6 px-2 bg-gray-800 border border-gray-700 rounded text-[9px] text-cyan-400 hover:bg-gray-700 disabled:opacity-40 disabled:cursor-not-allowed disabled:hover:bg-gray-800"
            >
              {showKeypad ? 'Hide' : 'Cash'}
            </button>
          </div>
        </div>
      </div>

      {/* Right: Keypad for reconciliation */}
      {showKeypad && (
        <div className="w-[160px] p-1.5 border-l border-gray-800 bg-gray-900/30 flex flex-col gap-1">
          <NumericKeypad
            value={cashInput}
            onChange={setCashInput}
            placeholder="Actual Cash"
            showDecimal={true}
            maxLen={10}
          />
          <button
            onClick={handleReconcile}
            disabled={isSent || !selectedEmp || !cashInput}
            className="h-7 bg-cyan-700 hover:bg-cyan-600 disabled:bg-gray-800 disabled:text-gray-600 rounded text-white text-xs font-bold active:scale-95 transition-colors"
          >
            RECONCILE
          </button>
        </div>
      )}

      {/* Item View Modal */}
      {selectedViewEmp && (
        <div className="absolute inset-0 bg-black/80 flex items-center justify-center z-50 p-4">
          <div className="bg-gray-900 border border-gray-700 rounded w-[400px] max-h-[250px] flex flex-col shadow-2xl">
            <div className="p-2 border-b border-gray-800 flex justify-between items-center bg-gray-900/50">
              <div className="text-[10px] text-green-400 font-bold uppercase">Items: {selectedViewEmp.name}</div>
              <button onClick={() => setViewItemsEmpId(null)} className="text-gray-500 hover:text-white text-xs">✕</button>
            </div>
            <div className="flex-1 overflow-y-auto p-1">
              <table className="w-full text-[9px]">
                <thead className="sticky top-0 bg-gray-900 text-gray-500 uppercase">
                  <tr>
                    <th className="px-1 text-left">Barcode</th>
                    <th className="px-1 text-right">Price</th>
                    <th className="px-1 text-right">Total</th>
                  </tr>
                </thead>
                <tbody className="text-gray-300 font-mono">
                  {selectedViewEmp.entries.flatMap(e => e.items).map((item, idx) => (
                    <tr key={idx} className="border-t border-gray-800/50">
                      <td className="px-1 text-cyan-400">{item.barcode}</td>
                      <td className="px-1 text-right">₱{item.price.toFixed(2)}</td>
                      <td className="px-1 text-right text-yellow-400">₱{item.lineTotal.toFixed(2)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
