import { useState, useEffect } from 'react';
import type { Employee, SalesItem, SalesEntry as SalesEntryType } from '../types';
import { genId, padBarcode, getWeekKey } from '../storage';
import NumericKeypad from './NumericKeypad';
import { ShoppingCart, Send, Trash2, X, AlertTriangle, Edit2, Mail } from 'lucide-react';

interface Props {
  employees: Employee[];
  onSubmit: (entry: SalesEntryType) => void;
  existingOpenEntry: SalesEntryType | null;
  onEmployeeChange: (id: string) => void;
  selectedEmployeeId: string;
  selectedDate: string;
  isSent: boolean;
}

type Field = 'barcode' | 'price' | 'confirmation';

export default function SalesEntryScreen({ employees, onSubmit, existingOpenEntry, onEmployeeChange, selectedEmployeeId, selectedDate, isSent }: Props) {
  const [employeeId, setEmployeeId] = useState(selectedEmployeeId);
  const [barcode, setBarcode] = useState('');
  const [price, setPrice] = useState('');
  const [discount, setDiscount] = useState('0');
  const [quantity, setQuantity] = useState(1);
  const [items, setItems] = useState<SalesItem[]>([]);
  const [activeField, setActiveField] = useState<Field>('barcode');
  const [msg, setMsg] = useState('');
  const [isReviewing, setIsReviewing] = useState(false);
  const [manualTotal, setManualTotal] = useState('');
  const [showMismatch, setShowMismatch] = useState(false);
  const [showSubmitConfirm, setShowSubmitConfirm] = useState(false);
  const [confirmAmount, setConfirmAmount] = useState(0);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [isSuccess, setIsSuccess] = useState(false);
  const [editingItemId, setEditingItemId] = useState<string | null>(null);

  const buildDraftEntry = (itemsList: SalesItem[]): SalesEntryType => ({
    id: existingOpenEntry?.id || genId(),
    employeeId,
    employeeName: selectedEmployee?.name || 'Unknown',
    items: itemsList,
    grandTotal: Math.round(itemsList.reduce((s, i) => s + i.lineTotal, 0) * 100) / 100,
    status:
      existingOpenEntry?.status === 'CLOSED' &&
      JSON.stringify(itemsList) !== JSON.stringify(existingOpenEntry.items)
        ? 'OPEN'
        : existingOpenEntry?.status || 'OPEN',
    date: selectedDate,
    weekKey: getWeekKey(new Date(selectedDate)),
  });

  const clearForm = () => {
    setBarcode('');
    setPrice('');
    setDiscount('0');
    setQuantity(1);
    setActiveField('barcode');
    setEditingItemId(null);
  };

  // Sync when switching salesman, date, or entry — not on every save (same entry id)
  useEffect(() => {
    setEmployeeId(selectedEmployeeId);
    setEditingItemId(null);
    setBarcode('');
    setPrice('');
    setDiscount('0');
    setQuantity(1);
    setActiveField('barcode');
    if (existingOpenEntry) {
      setItems(existingOpenEntry.items);
    } else {
      setItems([]);
    }
  }, [existingOpenEntry?.id, selectedEmployeeId, selectedDate]);

  const selectedEmployee = employees.find((e) => e.id === employeeId);

  const handleEmployeeChange = (id: string) => {
    if (isReviewing) return; // Prevent switch while reviewing
    if (isSent) {
      setEmployeeId(id);
      onEmployeeChange(id);
      return;
    }
    // If there are unsaved items, we should ideally save them to the current employee's open entry first
    if (items.length > 0) {
      onSubmit(buildDraftEntry(items));
    }
    setEditingItemId(null);
    setEmployeeId(id);
    onEmployeeChange(id);
  };

  const keypadVal = activeField === 'barcode' ? barcode : activeField === 'price' ? price : manualTotal;
  const keypadSet = activeField === 'barcode' ? setBarcode : activeField === 'price' ? setPrice : setManualTotal;
  const keypadDecimal = true;
  const keypadMax = activeField === 'barcode' ? 13 : 10;

  const addItem = () => {
    if (isSent) {
      setMsg('Date locked (Invoice Sent)');
      return;
    }
    const p = parseFloat(price);
    if (!barcode || isNaN(p) || p <= 0) {
      setMsg('Invalid entry');
      return;
    }
    const discVal = parseFloat(discount) || 0;
    const lineTotal = (p * quantity) * (1 - discVal / 100);

    const item: SalesItem = {
      id: editingItemId || genId(),
      barcode: padBarcode(barcode),
      price: p,
      quantity,
      discount: discVal,
      lineTotal: Math.round(lineTotal * 100) / 100,
    };

    const nextItems = editingItemId
      ? items.map((i) => (i.id === editingItemId ? item : i))
      : [...items, item];
    setItems(nextItems);
    onSubmit(buildDraftEntry(nextItems));
    clearForm();
    setMsg(editingItemId ? 'Item updated' : '');
    setTimeout(() => setMsg(''), 1500);
  };

  const removeItem = (id: string) => {
    if (isSent) {
      setMsg('Date locked (Invoice Sent)');
      return;
    }
    const nextItems = items.filter((i) => i.id !== id);
    setItems(nextItems);
    if (editingItemId === id) clearForm();
    if (nextItems.length > 0) onSubmit(buildDraftEntry(nextItems));
  };

  const cancelEdit = () => {
    clearForm();
    setMsg('');
  };

  const editItem = (item: SalesItem) => {
    if (isSent) {
      setMsg('Date locked (Invoice Sent)');
      return;
    }
    if (isReviewing) return;
    setEditingItemId(item.id);
    setBarcode(item.barcode.replace(/^0+/, ''));
    setPrice(item.price.toString());
    setDiscount(item.discount.toString());
    setQuantity(item.quantity);
    setActiveField('barcode');
    setMsg('Editing — tap UPDATE ITEM when done');
    setTimeout(() => setMsg(''), 2000);
  };

  const grandTotal = items.reduce((s, i) => s + i.lineTotal, 0);
  const formatPeso = (amount: number) => `₱${amount.toLocaleString('en-PH', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;

  const isModified = existingOpenEntry ? JSON.stringify(items) !== JSON.stringify(existingOpenEntry.items) : items.length > 0;
  const isAlreadyClosed = existingOpenEntry?.status === 'CLOSED' && !isModified;
  const reviewItems = isSuccess || isAlreadyClosed ? existingOpenEntry?.items || [] : items;
  const reviewQty = reviewItems.reduce((s, i) => s + i.quantity, 0);

  const startSubmit = () => {
    if (isSent) {
      setMsg('Date locked (Invoice Sent)');
      return;
    }
    if (items.length === 0) {
      setMsg('Add items first');
      return;
    }
    if (isAlreadyClosed) {
      setIsReviewing(true); // Still allow viewing the review if already closed
      return;
    }
    setShowSubmitConfirm(false);
    setShowMismatch(false);
    setIsReviewing(true);
    setIsSuccess(false);
    setActiveField('confirmation');
    setManualTotal('');
    setMsg('Review your items and confirm total');
  };

  const cancelReview = () => {
    setShowSubmitConfirm(false);
    setShowMismatch(false);
    setIsReviewing(false);
    setIsSuccess(false);
    setActiveField('barcode');
    setMsg('');
  };

  const doSubmit = () => {
    if (isSent) {
      setMsg('Date locked (Invoice Sent)');
      return;
    }
    if (isSubmitting) return;
    setIsSubmitting(true);
    const entry: SalesEntryType = {
      id: existingOpenEntry?.id || genId(),
      employeeId,
      employeeName: selectedEmployee?.name || 'Unknown',
      items,
      grandTotal,
      manualTotal: parseFloat(manualTotal),
      status: 'CLOSED',
      date: selectedDate,
      weekKey: getWeekKey(new Date(selectedDate)),
    };
    onSubmit(entry);
    setItems([]);
    setBarcode('');
    setPrice('');
    setDiscount('0');
    setQuantity(1);
    setIsSuccess(true);
    setManualTotal('');
    setMsg('Submitted!');
    setTimeout(() => {
      setMsg('');
      setIsSubmitting(false);
    }, 1500);
  };

  const finalSubmit = () => {
    if (isSent) {
      setMsg('Date locked (Invoice Sent)');
      return;
    }
    if (isSubmitting) return;
    const inputTotal = parseFloat(manualTotal) || 0;
    const sysTotal = Math.round(grandTotal * 100) / 100;

    if (Math.abs(inputTotal - sysTotal) > 0.01) {
      setShowMismatch(true);
      setMsg('Mismatch!');
      return;
    }

    setConfirmAmount(sysTotal);
    setShowSubmitConfirm(true);
  };

  const confirmSubmitYes = () => {
    setShowSubmitConfirm(false);
    doSubmit();
  };

  const fieldBtn = (field: Field, label: string, val: string, extraClass = '') => (
    <button
      type="button"
      onClick={() => !isSent && setActiveField(field)}
      disabled={isSent}
      className={`flex items-center justify-between px-1.5 py-1 rounded text-[10px] w-full shrink-0 transition-colors ${
        activeField === field
          ? 'bg-cyan-900/60 border border-cyan-500'
          : 'bg-gray-800/60 border border-gray-700 hover:border-gray-500'
      } ${isSent ? 'opacity-40 cursor-not-allowed' : ''} ${extraClass}`}
    >
      <span className="text-gray-400 uppercase tracking-wider shrink-0">{label}</span>
      <span className="font-mono text-green-400 text-xs truncate text-right">{val || '-'}</span>
    </button>
  );

  return (
    <div className="w-[480px] h-[288px] bg-gray-950 flex select-none overflow-hidden">
      {/* Left: Form + Items */}
      <div className="flex-1 flex flex-col min-w-0 p-1.5 gap-1 relative">
        {isSent && (
          <div className="absolute top-1 right-1 z-40 bg-green-900/60 border border-green-500 rounded px-1.5 py-0.5 flex items-center gap-1 shadow-lg animate-pulse">
            <Mail className="text-green-400" size={10} />
            <span className="text-green-400 text-[8px] font-black tracking-widest uppercase">Invoice Sent</span>
          </div>
        )}
        {/* Employee dropdown */}
        <div className="flex items-center gap-1">
          <span className="text-[9px] text-gray-500 uppercase w-12 shrink-0">Salesman</span>
          <select
            value={employeeId}
            onChange={(e) => handleEmployeeChange(e.target.value)}
            className="flex-1 bg-gray-800 border border-gray-700 rounded px-1 py-0.5 text-xs text-green-400 focus:border-cyan-500 outline-none"
          >
            {employees.map((emp) => (
              <option key={emp.id} value={emp.id}>{emp.name}</option>
            ))}
          </select>
        </div>

        {/* Field selectors */}
        <div className="flex flex-col gap-0.5">
          {fieldBtn('barcode', 'Barcode', barcode ? padBarcode(barcode) : '')}
        </div>

        {/* Items list */}
        <div className="flex-1 overflow-y-auto min-h-0 border border-gray-800 rounded bg-gray-900/50 custom-scrollbar relative">
          {isAlreadyClosed && (
            <div className="absolute top-2 left-1/2 -translate-x-1/2 z-10 bg-green-500/90 text-white text-[8px] px-2 py-0.5 rounded-full font-bold uppercase tracking-tighter shadow-lg pointer-events-none flex items-center gap-1">
              <Send size={8} /> Posted
            </div>
          )}
          {items.length === 0 ? (
            <div className="h-full flex items-center justify-center text-gray-700 text-[10px] italic">
              No items added
            </div>
          ) : (
            <div className="divide-y divide-gray-800/50">
              {items.map((item) => (
                <div
                  key={item.id}
                  className={`p-1.5 flex flex-col gap-0.5 hover:bg-gray-800/30 group ${
                    editingItemId === item.id ? 'bg-cyan-900/30 ring-1 ring-cyan-600/60 rounded' : ''
                  }`}
                >
                  <div className="flex justify-between items-start">
                    <span className="text-[9px] text-cyan-400 font-mono leading-none">{item.barcode}</span>
                    <div className="flex gap-2">
                      <button 
                        onClick={() => editItem(item)}
                        className="text-gray-500 hover:text-cyan-400 transition-colors"
                        title="Edit"
                      >
                        <Edit2 size={12} />
                      </button>
                      <button 
                        onClick={() => removeItem(item.id)}
                        className="text-gray-500 hover:text-red-500 transition-colors"
                        title="Delete"
                      >
                        <Trash2 size={12} />
                      </button>
                    </div>
                  </div>
                  <div className="flex justify-between items-end">
                    <div className="text-[8px] text-gray-500 leading-none">
                      ₱{item.price.toFixed(2)} 
                      {item.discount > 0 && <span className="text-red-400 ml-1">-{item.discount}%</span>}
                    </div>
                    <div className="text-[10px] text-green-400 font-bold leading-none">
                      ₱{item.lineTotal.toFixed(2)}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        <div className="p-2 bg-gray-950 border-t border-gray-800 flex flex-col gap-1.5">
          <div className="flex justify-between items-center">
            <span className="text-[10px] text-gray-500 uppercase">Grand Total</span>
            <span className="text-sm text-yellow-500 font-bold font-mono">₱{grandTotal.toFixed(2)}</span>
          </div>
          <button
            onClick={startSubmit}
            disabled={items.length === 0 || isAlreadyClosed || isSent}
            className={`w-full py-1.5 rounded text-[10px] font-bold uppercase tracking-widest transition-all shadow-lg ${
              isAlreadyClosed 
                ? 'bg-green-900/40 text-green-400 border border-green-800 cursor-default shadow-none' 
                : 'bg-cyan-600 hover:bg-cyan-500 disabled:bg-gray-800 disabled:text-gray-600 text-white shadow-cyan-900/20 active:scale-95'
            }`}
          >
            {isAlreadyClosed ? '✓ Submitted Successfully' : 'Review'}
          </button>
        </div>

        {/* Review items overlay */}
        {isReviewing && (
          <div className="absolute inset-0 bg-gray-950 z-20 flex flex-col p-2">
            <div className="flex justify-between items-center border-b border-gray-800 pb-1 mb-2">
              <span className="text-[10px] text-yellow-500 font-bold uppercase">
                {isSuccess || isAlreadyClosed ? 'Submission Summary' : 'Review Sales'}
              </span>
              <button onClick={cancelReview} className="text-gray-500 hover:text-white">
                <X size={14} />
              </button>
            </div>
            
            <div className="flex-1 overflow-y-auto text-[9px] mb-2 custom-scrollbar">
              <table className="w-full text-left">
                <thead className="text-gray-500 uppercase border-b border-gray-800">
                  <tr>
                    <th className="pb-1">Item</th>
                    <th className="pb-1 text-right">Total</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-800/50">
                  {reviewItems.map(item => (
                    <tr key={item.id}>
                      <td className="py-1 text-cyan-400 font-mono truncate max-w-[80px]">{item.barcode}</td>
                      <td className="py-1 text-right text-green-400 font-bold">₱{item.lineTotal.toFixed(2)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>

            <div className="flex flex-col gap-1 mt-auto bg-gray-900/50 p-1.5 rounded border border-gray-800">
              <div className="flex justify-between text-[10px] font-bold border-b border-gray-800 pb-1 mb-1">
                <span className="text-gray-400 uppercase">System Total</span>
                <span className="text-yellow-500">₱{(isSuccess || isAlreadyClosed ? existingOpenEntry?.grandTotal || 0 : grandTotal).toFixed(2)}</span>
              </div>
              <div className="flex justify-between text-[9px] font-bold border-b border-gray-800/60 pb-1 mb-1">
                <span className="text-gray-500 uppercase">Total Qty</span>
                <span className="text-cyan-400 font-mono">{reviewQty}</span>
              </div>
              
              <div className="flex flex-col gap-1">
                {isSuccess || isAlreadyClosed ? (
                  <div className="flex flex-col items-center gap-2 py-1">
                    <div className="flex items-center gap-2 text-green-400 font-bold uppercase text-[10px] animate-bounce">
                      <Send size={12} /> Successfully Submitted
                    </div>
                    <button 
                      onClick={cancelReview}
                      className="w-full py-2 bg-gray-800 hover:bg-gray-700 text-white rounded text-[10px] font-bold uppercase tracking-widest transition-all active:scale-95"
                    >
                      Done / Close Review
                    </button>
                  </div>
                ) : (
                  <>
                    <div className="flex items-center justify-between">
                      <span className="text-[8px] text-gray-500 uppercase font-bold">Input Total to Confirm:</span>
                      <span className="text-xs font-mono text-green-400 font-bold">
                        {manualTotal ? `₱${manualTotal}` : '₱0.00'}
                      </span>
                    </div>
                    <button 
                      onClick={finalSubmit}
                      disabled={!manualTotal || isSubmitting}
                      className="w-full py-2 bg-green-600 hover:bg-green-500 disabled:bg-gray-800 disabled:text-gray-600 text-white rounded text-[10px] font-bold uppercase tracking-widest transition-all active:scale-95 shadow-lg shadow-green-900/40"
                    >
                      {isSubmitting ? 'Submitting...' : 'Confirm & Submit Sale'}
                    </button>
                    <button 
                      onClick={cancelReview}
                      className="w-full py-1 text-[8px] text-gray-500 uppercase font-bold hover:text-gray-300 transition-colors"
                    >
                      Back / Edit Items
                    </button>
                  </>
                )}
              </div>
            </div>
          </div>
        )}

        {showSubmitConfirm && (
          <div className="absolute inset-0 bg-black/80 z-40 flex items-center justify-center p-3">
            <div className="w-full max-w-[320px] bg-gray-900 border border-cyan-500/40 rounded p-3 shadow-xl">
              <div className="text-[10px] text-yellow-400 font-bold uppercase mb-1">Confirm Submission</div>
              <div className="text-[10px] text-gray-200 mb-3">
                Are you sure you want to submit <span className="text-green-400 font-bold">{formatPeso(confirmAmount)}</span> total sales?
              </div>
              <div className="flex gap-2">
                <button
                  onClick={confirmSubmitYes}
                  className="flex-1 py-2 bg-green-600 hover:bg-green-500 text-white rounded text-[10px] font-bold uppercase tracking-widest active:scale-95 transition-transform"
                >
                  Yes
                </button>
                <button
                  onClick={() => setShowSubmitConfirm(false)}
                  className="flex-1 py-2 bg-gray-800 hover:bg-gray-700 text-white rounded text-[10px] font-bold uppercase tracking-widest active:scale-95 transition-transform"
                >
                  Back
                </button>
              </div>
            </div>
          </div>
        )}

        {/* Mismatch Alert Overlay */}
        {showMismatch && (
          <div className="absolute inset-0 bg-red-950/95 z-30 flex flex-col items-center justify-center p-4 text-center animate-pulse-slow">
            <AlertTriangle className="text-white mb-2" size={32} />
            <h3 className="text-white font-bold text-xs uppercase mb-1">Total Mismatch!</h3>
            <p className="text-red-200 text-[9px] mb-4">
              System: ₱{grandTotal.toFixed(2)}<br/>
              Input: ₱{parseFloat(manualTotal).toFixed(2)}
            </p>
            <button 
              onClick={() => setShowMismatch(false)}
              className="bg-white text-red-900 px-4 py-1.5 rounded text-[10px] font-bold uppercase active:scale-95 transition-transform"
            >
              I'll Check Again
            </button>
          </div>
        )}

        {msg && (
          <div className={`text-[9px] text-center font-bold ${msg === 'Submitted!' ? 'text-green-400' : 'text-red-400'}`}>
            {msg}
          </div>
        )}
      </div>

      {/* Right: keypad top, price/discount/add item pinned to bottom */}
      <div className="w-[170px] shrink-0 h-full p-1.5 border-l border-gray-800 bg-gray-900/30 flex flex-col min-h-0 overflow-hidden">
        <div className={`flex-1 min-h-0 flex flex-col overflow-hidden ${isSent ? 'pointer-events-none opacity-40' : ''}`}>
          <NumericKeypad
            value={keypadVal}
            onChange={keypadSet}
            placeholder={activeField.toUpperCase()}
            showDecimal={keypadDecimal}
            maxLen={keypadMax}
            dense
            fillHeight
          />
        </div>
        <div
          className={`shrink-0 pt-2 mt-1 border-t border-gray-800 flex flex-col gap-1.5 ${
            isReviewing || isSent ? 'pointer-events-none opacity-40' : ''
          }`}
        >
          <div className="grid grid-cols-2 gap-1.5">
            <button
              type="button"
              onClick={() => !isSent && setActiveField('price')}
              disabled={isSent}
              className={`flex flex-col justify-center px-2 py-1 h-[50px] rounded border text-left transition-colors ${
                activeField === 'price'
                  ? 'bg-cyan-900/60 border-cyan-500'
                  : 'bg-gray-800/60 border-gray-700 hover:border-gray-500'
              }`}
            >
              <span className="text-[7px] text-gray-500 uppercase tracking-wider">Price</span>
              <span className="font-mono text-green-400 text-sm truncate leading-tight">
                {price ? `₱${price}` : '—'}
              </span>
            </button>
            <div className="flex flex-col justify-center px-2 py-1 h-[50px] rounded border bg-gray-800/60 border-gray-700">
              <span className="text-[7px] text-gray-500 uppercase tracking-wider leading-none mb-0.5">Disc%</span>
              <select
                value={discount}
                onChange={(e) => setDiscount(e.target.value)}
                className="w-full bg-gray-900/90 border border-gray-700 rounded px-1 py-1 text-green-400 text-[11px] font-mono outline-none cursor-pointer focus:border-cyan-600"
              >
                <option value="0" className="bg-gray-900">0%</option>
                <option value="10" className="bg-gray-900">10%</option>
                <option value="20" className="bg-gray-900">20%</option>
              </select>
            </div>
          </div>
          <button
            type="button"
            onClick={addItem}
            className="w-full h-10 shrink-0 bg-green-800 hover:bg-green-700 rounded text-white text-[11px] font-bold flex items-center justify-center gap-1 active:scale-95 transition-transform shadow-lg shadow-green-950/30"
          >
            <ShoppingCart size={15} /> {editingItemId ? 'UPDATE ITEM' : 'ADD ITEM'}
          </button>
          {editingItemId && (
            <button
              type="button"
              onClick={cancelEdit}
              className="w-full h-5 text-[8px] text-gray-500 hover:text-gray-300 font-bold uppercase tracking-wide"
            >
              Cancel edit
            </button>
          )}
        </div>
      </div>
    </div>
  );
}
