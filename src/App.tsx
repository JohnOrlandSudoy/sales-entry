import { useState, useEffect, useCallback } from 'react';
import type { Screen, AppSettings, SalesEntry, CashReconciliation } from './types';
import { loadSettings, saveSettings, loadSales, saveSales, loadReconciliation, saveReconciliation } from './storage';
import LockScreen from './components/LockScreen';
import Header from './components/Header';
import SalesEntryScreen from './components/SalesEntry';
import Dashboard from './components/Dashboard';
import Settings from './components/Settings';
import WeeklyHistory from './components/WeeklyHistory';
import MasterPinChallenge from './components/MasterPinChallenge';

const toDateInput = (date: Date) => {
  const tzOffset = date.getTimezoneOffset() * 60000;
  return new Date(date.getTime() - tzOffset).toISOString().slice(0, 10);
};

const getDateBounds = () => {
  const now = new Date();
  const maxDate = toDateInput(now);
  const minDate = toDateInput(new Date(now.getFullYear(), now.getMonth(), now.getDate() - 7));
  return { minDate, maxDate };
};

/** Flat invoice rows sorted by salesman, then date. */
function buildInvoiceReviewRows(entries: SalesEntry[]) {
  const rows = entries.flatMap((entry) => {
    const lastIdx = entry.items.length - 1;
    return entry.items.map((item, itemIdx) => ({
      entry,
      item,
      itemIdx,
      isLastInSale: itemIdx === lastIdx,
    }));
  });

  rows.sort((a, b) => {
    const byName = a.entry.employeeName.localeCompare(b.entry.employeeName);
    if (byName !== 0) return byName;
    return a.entry.date.localeCompare(b.entry.date);
  });

  const totalQtyBySalesman = new Map<string, number>();
  for (const { entry, item } of rows) {
    const name = entry.employeeName;
    totalQtyBySalesman.set(name, (totalQtyBySalesman.get(name) ?? 0) + item.quantity);
  }

  const lastRowBySalesman = new Map<string, number>();
  rows.forEach((r, i) => lastRowBySalesman.set(r.entry.employeeName, i));

  return rows.map((r, i) => ({
    ...r,
    isLastForSalesman: lastRowBySalesman.get(r.entry.employeeName) === i,
    salesmanTotalQty: totalQtyBySalesman.get(r.entry.employeeName) ?? 0,
  }));
}

function App() {
  const [unlocked, setUnlocked] = useState(false);
  const [screen, setScreen] = useState<Screen>('sales');
  const [settings, setSettingsState] = useState<AppSettings>(loadSettings);
  const [sales, setSales] = useState<SalesEntry[]>(loadSales);
  const [reconciliation, setReconciliation] = useState<CashReconciliation[]>(loadReconciliation);
  const [dateTime, setDateTime] = useState('');
  const [masterPinTarget, setMasterPinTarget] = useState<'settings' | 'email' | null>(null);
  const [salesEmployeeId, setSalesEmployeeId] = useState<string | undefined>(undefined);
  const [emailMsg, setEmailMsg] = useState('');
  const [showPromoReview, setShowPromoReview] = useState(false);
  const [selectedDate, setSelectedDate] = useState(() => toDateInput(new Date()));

  // Clock
  useEffect(() => {
    const update = () => {
      const now = new Date();
      setDateTime(
        now.toLocaleDateString('en-US', { month: 'short', day: '2-digit' }) +
          ' ' +
          now.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false })
      );
    };
    update();
    const id = setInterval(update, 1000);
    return () => clearInterval(id);
  }, []);

  // Persist
  useEffect(() => { saveSettings(settings); }, [settings]);
  useEffect(() => { saveSales(sales); }, [sales]);
  useEffect(() => { saveReconciliation(reconciliation); }, [reconciliation]);

  const handleUnlock = useCallback(() => setUnlocked(true), []);

  const handleNavigate = useCallback((s: Screen) => {
    if (s === 'settings') {
      setMasterPinTarget('settings');
      return;
    }
    setScreen(s);
  }, []);

  const handleSettingsClick = useCallback(() => {
    setMasterPinTarget('settings');
  }, []);

  const handleEmailClick = useCallback(() => {
    setMasterPinTarget('email');
  }, []);

  const handleMasterPinSuccess = useCallback(() => {
    if (masterPinTarget === 'settings') {
      setScreen('settings');
    } else if (masterPinTarget === 'email') {
      setShowPromoReview(true);
    }
    setMasterPinTarget(null);
  }, [masterPinTarget]);

  const handleSendEmail = useCallback(() => {
    setSettingsState((prev) => ({
      ...prev,
      sentDates: (prev.sentDates || []).includes(selectedDate)
        ? (prev.sentDates || [])
        : [...(prev.sentDates || []), selectedDate],
    }));
    setEmailMsg(
      settings.senderEmail
        ? `Invoice sent from ${settings.senderEmail}!`
        : 'Invoice sent to client!'
    );
    setTimeout(() => setEmailMsg(''), 2000);
    setShowPromoReview(false);
  }, [selectedDate, settings.senderEmail]);

  const handleMasterPinCancel = useCallback(() => {
    setMasterPinTarget(null);
  }, []);

  const handleSubmitSales = useCallback((entry: SalesEntry) => {
    setSales((prev) => {
      const existing = prev.findIndex((e) => e.id === entry.id);
      if (existing >= 0) {
        const updated = [...prev];
        updated[existing] = entry;
        return updated;
      }
      return [...prev, entry];
    });
  }, []);

  const handleReconcile = useCallback((rec: CashReconciliation) => {
    setReconciliation((prev) => {
      const existing = prev.findIndex((r) => r.employeeId === rec.employeeId);
      if (existing >= 0) {
        const updated = [...prev];
        updated[existing] = rec;
        return updated;
      }
      return [...prev, rec];
    });
  }, []);

  const handleSaveSettings = useCallback((s: AppSettings) => {
    setSettingsState(s);
  }, []);

  const handleClearWeek = useCallback((weekKey: string) => {
    setSales((prev) => prev.filter((e) => e.weekKey !== weekKey));
    setReconciliation((prev) => {
      const entriesToKeep = sales.filter((e) => e.weekKey !== weekKey);
      const keepIds = new Set(entriesToKeep.map((e) => e.employeeId));
      return prev.filter((r) => keepIds.has(r.employeeId));
    });
  }, [sales]);

  const handleGoToSales = useCallback((employeeId: string) => {
    setSalesEmployeeId(employeeId);
    setScreen('sales');
  }, []);

  const handleDateChange = useCallback((next: string) => {
    const { minDate, maxDate } = getDateBounds();
    let val = next;
    if (val > maxDate) val = maxDate;
    if (val < minDate) val = minDate;
    if ((settings.sentDates || []).includes(val) && val !== selectedDate) return;
    setSelectedDate(val);
  }, [settings.sentDates, selectedDate]);

  // Find entry for selected employee and date
  const currentSalesEmployeeId = salesEmployeeId || settings.employees[0]?.id;
  const openEntry = currentSalesEmployeeId
    ? sales.find((e) => e.employeeId === currentSalesEmployeeId && e.date === selectedDate) || null
    : null;

  const isSent = settings.sentDates?.includes(selectedDate) || false;
  const { minDate, maxDate } = getDateBounds();

  return (
    <div className="min-h-screen bg-black flex items-center justify-center p-4">
      <div className="relative w-[480px] h-[328px] bg-gray-950 border border-gray-800 rounded-xl overflow-hidden shadow-[0_0_50px_rgba(0,0,0,0.8)] flex flex-col">
        {!unlocked ? (
          <LockScreen onUnlock={handleUnlock} correctPin={settings.systemPin} />
        ) : (        
          <>
            {/* Header */}
            <div className="shrink-0">
              <Header
                currentScreen={screen}
                onNavigate={handleNavigate}
                onSettings={handleSettingsClick}
                onEmail={handleEmailClick}
                dateTime={dateTime}
                selectedDate={selectedDate}
                onDateChange={handleDateChange}
                minDate={minDate}
                maxDate={maxDate}
              />
            </div>

            {/* Content area */}
            <div className="flex-1 relative overflow-hidden">
              {screen === 'sales' && (
                <SalesEntryScreen
                  employees={settings.employees}
                  onSubmit={handleSubmitSales}
                  existingOpenEntry={openEntry}
                  onEmployeeChange={(id) => setSalesEmployeeId(id)}
                  selectedEmployeeId={currentSalesEmployeeId}
                  selectedDate={selectedDate}
                  isSent={isSent}
                />
              )}
              {screen === 'dashboard' && (
                <Dashboard
                  entries={sales}
                  employees={settings.employees}
                  reconciliation={reconciliation}
                  onReconcile={handleReconcile}
                  onGoToSales={handleGoToSales}
                  selectedDate={selectedDate}
                  isSent={isSent}
                />
              )}
              {screen === 'settings' && (
                <Settings
                  settings={settings}
                  onSave={handleSaveSettings}
                  onBack={() => setScreen('dashboard')}
                />
              )}
              {screen === 'history' && (
                <WeeklyHistory
                  entries={sales}
                  onClearWeek={handleClearWeek}
                  masterPin={settings.masterPin}
                  onBack={() => setScreen('dashboard')}
                />
              )}
            </div>

            {/* Footer / Prototype Mention */}
            <div className="h-4 bg-gray-900/50 border-t border-gray-800/50 flex items-center justify-center px-3 shrink-0">
              <span className="text-[7px] text-gray-600 tracking-widest uppercase font-medium">
                Prototype UI for <span className="text-gray-500 font-bold">"DAILY SALES ENTRY"</span>
              </span>
            </div>

            {/* Master PIN overlay */}
            {masterPinTarget && (
              <MasterPinChallenge
                correctPin={settings.masterPin}
                onSuccess={handleMasterPinSuccess}
                onCancel={handleMasterPinCancel}
                label={masterPinTarget === 'email' ? 'Master PIN for Email/Invoice' : 'Master PIN Required'}
              />
            )}

            {/* Email toast */}
            {emailMsg && (
              <div className="absolute top-8 left-1/2 -translate-x-1/2 bg-green-800 border border-green-600 rounded px-3 py-1 text-green-300 text-xs font-bold z-40 animate-pulse">
                {emailMsg}
              </div>
            )}

            {/* Promo Head Review Overlay */}
            {showPromoReview && (
              <div className="absolute inset-0 bg-black/90 flex items-center justify-center z-50 p-4">
                <div className="bg-gray-900 border border-gray-700 rounded w-[440px] h-[260px] flex flex-col shadow-2xl overflow-hidden">
                  <div className="p-2 border-b border-gray-800 flex justify-between items-center bg-gray-950">
                    <div className="text-[10px] text-yellow-400 font-bold uppercase tracking-widest">
                      Review by: {settings.promoHeadName} - CSV Invoice
                    </div>
                    <button onClick={() => setShowPromoReview(false)} className="text-gray-500 hover:text-white">✕</button>
                  </div>
                  <div className="flex-1 p-0 overflow-auto bg-gray-950">
                <table className="w-full text-[7px] border-collapse">
                  <thead className="sticky top-0 bg-gray-900 text-yellow-500 uppercase font-bold border-b border-gray-800">
                    <tr>
                      <th className="p-1 text-left border-r border-gray-800">Date</th>
                      <th className="p-1 text-left border-r border-gray-800">Salesman</th>
                      <th className="p-1 text-left border-r border-gray-800">Barcode</th>
                      <th className="p-1 text-right border-r border-gray-800">Price</th>
                      <th className="p-1 text-right border-r border-gray-800">Qty</th>
                      <th className="p-1 text-right border-r border-gray-800">Tot Qty</th>
                      <th className="p-1 text-right border-r border-gray-800">Sys Total</th>
                      <th className="p-1 text-right">Manual</th>
                    </tr>
                  </thead>
                  <tbody className="text-cyan-400 font-mono">
                    {buildInvoiceReviewRows(sales).map(({ entry, item, itemIdx, isLastInSale, isLastForSalesman, salesmanTotalQty }) => (
                      <tr
                        key={`${entry.id}-${itemIdx}`}
                        className={`border-b border-gray-900 hover:bg-gray-900/50 ${isLastForSalesman ? 'border-b-gray-700' : ''}`}
                      >
                        <td className="p-1 border-r border-gray-800 text-gray-400">{entry.date}</td>
                        <td className="p-1 border-r border-gray-800 truncate max-w-[50px]">{entry.employeeName}</td>
                        <td className="p-1 border-r border-gray-800 text-[6px]">{item.barcode}</td>
                        <td className="p-1 border-r border-gray-800 text-right">{item.price.toFixed(2)}</td>
                        <td className="p-1 border-r border-gray-800 text-right">{item.quantity}</td>
                        <td className="p-1 border-r border-gray-800 text-right text-cyan-300 font-bold">
                          {isLastForSalesman ? salesmanTotalQty : ''}
                        </td>
                        <td className="p-1 border-r border-gray-800 text-right text-yellow-500 font-bold">
                          {isLastInSale ? entry.grandTotal.toFixed(2) : ''}
                        </td>
                        <td className="p-1 text-right text-green-500 font-bold">
                          {isLastInSale ? (entry.manualTotal ?? 0).toFixed(2) : ''}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
                  <div className="p-2 border-t border-gray-800 flex gap-2 bg-gray-950">
                    <button
                      onClick={() => setShowPromoReview(false)}
                      className="flex-1 h-8 bg-gray-800 hover:bg-gray-700 rounded text-gray-400 text-xs font-bold"
                    >
                      CANCEL
                    </button>
                    <button
                      onClick={handleSendEmail}
                      className="flex-1 h-8 bg-green-700 hover:bg-green-600 rounded text-white text-xs font-bold shadow-lg shadow-green-900/20 active:scale-95 transition-transform"
                    >
                      APPROVE & SEND EMAIL
                    </button>
                  </div>
                </div>
              </div>
            )}
          </>
        )}
      </div>
    </div>
  );
}

export default App;
