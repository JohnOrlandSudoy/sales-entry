import { useState, useEffect } from 'react';
import type { Employee, SalesItem, SalesEntry as SalesEntryType } from '../types';
import { genId, padBarcode, getWeekKey } from '../storage';
import NumericKeypad from './NumericKeypad';
import { Plus, Minus, ShoppingCart, Send, Trash2, X, AlertTriangle, Edit2 } from 'lucide-react';

interface Props {
  employees: Employee[];
  onSubmit: (entry: SalesEntryType) => void;
  existingOpenEntry: SalesEntryType | null;
  onEmployeeChange: (id: string) => void;
}

type Field = 'barcode' | 'price' | 'confirmation';

export default function SalesEntryScreen({ employees, onSubmit, existingOpenEntry, onEmployeeChange }: Props) {
  const today = new Date().toISOString().split('T')[0];
  const [employeeId, setEmployeeId] = useState(existingOpenEntry?.employeeId || (employees[0]?.id ?? ''));
  const [barcode, setBarcode] = useState('');
  const [price, setPrice] = useState('');
  const [discount, setDiscount] = useState('0');
  const [quantity, setQuantity] = useState(1);
  const [items, setItems] = useState<SalesItem[]>([]);
  const [activeField, setActiveField] = useState<Field>('barcode');
  const [msg, setMsg] = useState('');
  const [isReviewing, setIsReviewing] = useState(false);
  const [isConfirming, setIsConfirming] = useState(false);
  const [manualTotal, setManualTotal] = useState('');
  const [showMismatch, setShowMismatch] = useState(false);

  // Sync items when existingOpenEntry changes (e.g. employee switch)
  useEffect(() => {
    if (existingOpenEntry) {
      setItems(existingOpenEntry.items);
      setEmployeeId(existingOpenEntry.employeeId);
    } else {
      setItems([]);
    }
  }, [existingOpenEntry]);

  const selectedEmployee = employees.find((e) => e.id === employeeId);

  const handleEmployeeChange = (id: string) => {
    if (isConfirming || isReviewing) return; // Prevent switch while confirming/reviewing
    // If there are unsaved items, we should ideally save them to the current employee's open entry first
    if (items.length > 0) {
      const currentEntry: SalesEntryType = {
        id: existingOpenEntry?.id || genId(),
        employeeId,
        employeeName: selectedEmployee?.name || 'Unknown',
        items,
        grandTotal: Math.round(items.reduce((s, i) => s + i.lineTotal, 0) * 100) / 100,
        status: 'OPEN',
        date: today,
        weekKey: getWeekKey(new Date()),
      };
      onSubmit(currentEntry);
    }
    setEmployeeId(id);
    onEmployeeChange(id);
  };

  const keypadVal = activeField === 'barcode' ? barcode : activeField === 'price' ? price : manualTotal;
  const keypadSet = activeField === 'barcode' ? setBarcode : activeField === 'price' ? setPrice : setManualTotal;
  const keypadDecimal = activeField !== 'barcode';
  const keypadMax = activeField === 'barcode' ? 13 : 10;

  const onKeypadSubmit = () => {
    if (activeField === 'barcode') {
      if (barcode.length < 3) {
        setMsg('Invalid barcode');
        return;
      }
      setActiveField('price');
      setMsg('Input Price');
    } else if (activeField === 'price') {
      addItem();
    } else if (activeField === 'confirmation') {
      finalSubmit();
    }
  };

  const addItem = () => {
    const p = parseFloat(price);
    if (!barcode || isNaN(p) || p <= 0) {
      setMsg('Invalid entry');
      return;
    }
    const discVal = parseFloat(discount) || 0;
    const lineTotal = (p * quantity) * (1 - discVal / 100);

    const item: SalesItem = {
      id: genId(),
      barcode: padBarcode(barcode),
      price: p,
      quantity,
      discount: discVal,
      lineTotal: Math.round(lineTotal * 100) / 100,
    };

    setItems((prev) => [...prev, item]);
    setBarcode('');
    setPrice('');
    setDiscount('0');
    setQuantity(1);
    setActiveField('barcode');
    setMsg('');
  };

  const removeItem = (id: string) => {
    setItems((prev) => prev.filter((i) => i.id !== id));
  };

  const editItem = (item: SalesItem) => {
    if (isConfirming || isReviewing) return;
    // Load item back to form
    setBarcode(item.barcode.replace(/^0+/, '')); // Remove padding for editing
    setPrice(item.price.toString());
    setDiscount(item.discount.toString());
    setQuantity(item.quantity);
    // Remove from list
    removeItem(item.id);
    setActiveField('price');
    setMsg('Editing item...');
    setTimeout(() => setMsg(''), 1500);
  };

  const grandTotal = items.reduce((s, i) => s + i.lineTotal, 0);

  const startSubmit = () => {
    if (items.length === 0) {
      setMsg('Add items first');
      return;
    }
    setIsReviewing(true);
  };

  const proceedToConfirm = () => {
    setIsReviewing(false);
    setIsConfirming(true);
    setActiveField('confirmation');
    setManualTotal('');
    setMsg('Manual confirm: Input your total');
  };

  const cancelReview = () => {
    setIsReviewing(false);
    setIsConfirming(false);
    setActiveField('barcode');
    setMsg('');
  };

  const finalSubmit = () => {
    const inputTotal = parseFloat(manualTotal) || 0;
    const sysTotal = Math.round(grandTotal * 100) / 100;

    if (Math.abs(inputTotal - sysTotal) > 0.01) {
      setShowMismatch(true);
      setMsg(`Mismatch!`);
      return;
    }

    const entry: SalesEntryType = {
      id: existingOpenEntry?.id || genId(),
      employeeId,
      employeeName: selectedEmployee?.name || 'Unknown',
      items,
      grandTotal: sysTotal,
      manualTotal: inputTotal,
      status: 'CLOSED',
      date: today,
      weekKey: getWeekKey(new Date()),
    };
    onSubmit(entry);
    setItems([]);
    setBarcode('');
    setPrice('');
    setDiscount('0');
    setQuantity(1);
    setIsConfirming(false);
    setIsReviewing(false);
    setManualTotal('');
    setMsg('Submitted!');
    setTimeout(() => setMsg(''), 1500);
  };

  const fieldBtn = (field: Field, label: string, val: string) => (
    <button
      onClick={() => setActiveField(field)}
      className={`flex items-center justify-between px-1.5 py-1 rounded text-[10px] w-full transition-colors ${
        activeField === field
          ? 'bg-cyan-900/60 border border-cyan-500'
          : 'bg-gray-800/60 border border-gray-700 hover:border-gray-500'
      }`}
    >
      <span className="text-gray-400 uppercase tracking-wider">{label}</span>
      <span className="font-mono text-green-400 text-xs">{val || '-'}</span>
    </button>
  );

  return (
    <div className="w-[480px] h-[288px] bg-gray-950 flex select-none overflow-hidden">
      {/* Left: Form + Items */}
      <div className="flex-1 flex flex-col min-w-0 p-1.5 gap-1 relative">
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
          {fieldBtn('price', 'Price', price ? `₱${price}` : '')}
          
          {/* Discount Dropdown */}
          <div className="flex items-center justify-between px-1.5 py-1 rounded text-[10px] w-full bg-gray-800/60 border border-gray-700">
            <span className="text-gray-400 uppercase tracking-wider">Disc%</span>
            <select
              value={discount}
              onChange={(e) => setDiscount(e.target.value)}
              className="bg-transparent text-green-400 text-xs font-mono outline-none cursor-pointer"
            >
              <option value="0" className="bg-gray-900">None</option>
              <option value="10" className="bg-gray-900">10%</option>
              <option value="20" className="bg-gray-900">20%</option>
            </select>
          </div>

          {isConfirming && fieldBtn('confirmation', 'Confirm', manualTotal ? `₱${manualTotal}` : '')}
        </div>

        {/* Quantity spinbox */}
        <div className="flex items-center gap-1">
          <span className="text-[9px] text-gray-500 uppercase w-12 shrink-0">Qty</span>
          <button
            onClick={() => setQuantity((q) => Math.max(1, q - 1))}
            className="w-8 h-7 bg-gray-800 border border-gray-700 rounded flex items-center justify-center text-red-400 hover:bg-gray-700 active:scale-90"
          >
            <Minus size={12} />
          </button>
          <span className="w-8 text-center font-mono text-yellow-400 text-sm font-bold">{quantity}</span>
          <button
            onClick={() => setQuantity((q) => q + 1)}
            className="w-8 h-7 bg-gray-800 border border-gray-700 rounded flex items-center justify-center text-green-400 hover:bg-gray-700 active:scale-90"
          >
            <Plus size={12} />
          </button>
        </div>

        {/* Add button */}
        <button
          onClick={addItem}
          className="h-7 bg-green-800 hover:bg-green-700 rounded text-white text-xs font-bold flex items-center justify-center gap-1 active:scale-95 transition-transform"
        >
          <ShoppingCart size={12} /> ADD ITEM
        </button>

        {/* Items list */}
        <div className="flex-1 overflow-y-auto min-h-0 border border-gray-800 rounded bg-gray-900/50 custom-scrollbar">
          {items.length === 0 ? (
            <div className="h-full flex items-center justify-center text-gray-700 text-[10px] italic">
              No items added
            </div>
          ) : (
            <div className="divide-y divide-gray-800/50">
              {items.map((item) => (
                <div key={item.id} className="p-1.5 flex flex-col gap-0.5 hover:bg-gray-800/30 group">
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
                      {item.quantity}x ₱{item.price.toFixed(2)} 
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
          
          {isConfirming ? (
            <button
              onClick={finalSubmit}
              className="w-full py-1.5 bg-green-600 hover:bg-green-500 text-white rounded text-[10px] font-bold uppercase tracking-widest transition-colors shadow-lg shadow-green-900/20"
            >
              Confirm Total
            </button>
          ) : (
            <button
              onClick={startSubmit}
              disabled={items.length === 0}
              className="w-full py-1.5 bg-cyan-600 hover:bg-cyan-500 disabled:bg-gray-800 disabled:text-gray-600 text-white rounded text-[10px] font-bold uppercase tracking-widest transition-colors shadow-lg shadow-cyan-900/20"
            >
              Submit & Close
            </button>
          )}
        </div>

        {/* Review Overlay */}
        {isReviewing && (
          <div className="absolute inset-0 bg-gray-950 z-20 flex flex-col p-2">
            <div className="flex justify-between items-center border-b border-gray-800 pb-1 mb-2">
              <span className="text-[10px] text-yellow-500 font-bold uppercase">Review Sales</span>
              <button onClick={cancelReview} className="text-gray-500 hover:text-white">
                <X size={14} />
              </button>
            </div>
            
            <div className="flex-1 overflow-y-auto text-[9px] mb-2">
              <table className="w-full text-left">
                <thead className="text-gray-500 uppercase border-b border-gray-800">
                  <tr>
                    <th className="pb-1">Item</th>
                    <th className="pb-1 text-right">Qty</th>
                    <th className="pb-1 text-right">Total</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-gray-800/50">
                  {items.map(item => (
                    <tr key={item.id}>
                      <td className="py-1 text-cyan-400 font-mono truncate max-w-[80px]">{item.barcode}</td>
                      <td className="py-1 text-right text-gray-400">{item.quantity}</td>
                      <td className="py-1 text-right text-green-400 font-bold">₱{item.lineTotal.toFixed(2)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>

            <div className="flex flex-col gap-1">
              <div className="flex justify-between text-[10px] font-bold border-t border-gray-800 pt-1">
                <span className="text-gray-400 uppercase">Items: {items.length}</span>
                <span className="text-yellow-500">₱{grandTotal.toFixed(2)}</span>
              </div>
              <button 
                onClick={proceedToConfirm}
                className="w-full py-1.5 bg-green-600 hover:bg-green-500 text-white rounded text-[10px] font-bold uppercase"
              >
                Proceed to Close
              </button>
              <button 
                onClick={cancelReview}
                className="w-full py-1 bg-gray-800 hover:bg-gray-700 text-gray-400 rounded text-[9px] uppercase"
              >
                Back / Edit
              </button>
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

      {/* Right: Numeric Keypad */}
      <div className="w-[160px] p-1.5 border-l border-gray-800 bg-gray-900/30">
        <NumericKeypad
          value={keypadVal}
          onChange={keypadSet}
          placeholder={activeField.toUpperCase()}
          showDecimal={keypadDecimal}
          maxLen={keypadMax}
        />
      </div>
    </div>
  );
}
