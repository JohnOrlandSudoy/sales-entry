import { useState, useEffect } from 'react';
import type { Employee, SalesItem, SalesEntry as SalesEntryType } from '../types';
import { genId, padBarcode, getWeekKey } from '../storage';
import NumericKeypad from './NumericKeypad';
import { Plus, Minus, ShoppingCart, Send, Trash2 } from 'lucide-react';

interface Props {
  employees: Employee[];
  onSubmit: (entry: SalesEntryType) => void;
  existingOpenEntry: SalesEntryType | null;
  onEmployeeChange: (id: string) => void;
}

export default function SalesEntryScreen({ employees, onSubmit, existingOpenEntry, onEmployeeChange }: Props) {
  const today = new Date().toISOString().split('T')[0];
  const [employeeId, setEmployeeId] = useState(existingOpenEntry?.employeeId || (employees[0]?.id ?? ''));
  const [barcode, setBarcode] = useState('');
  const [price, setPrice] = useState('');
  const [discount, setDiscount] = useState('');
  const [quantity, setQuantity] = useState(1);
  const [items, setItems] = useState<SalesItem[]>([]);
  const [activeField, setActiveField] = useState<Field>('barcode');
  const [msg, setMsg] = useState('');

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
    // If there are unsaved items, we should ideally save them to the current employee's open entry first
    // but for simplicity in this flow, we'll just trigger the switch in parent
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

  const keypadVal = activeField === 'barcode' ? barcode : activeField === 'price' ? price : discount;
  const keypadSet = activeField === 'barcode' ? setBarcode : activeField === 'price' ? setPrice : setDiscount;
  const keypadDecimal = activeField !== 'barcode';
  const keypadMax = activeField === 'barcode' ? 13 : 10;

  const addItem = () => {
    if (!barcode && !price) {
      setMsg('Enter barcode or price');
      return;
    }
    const p = parseFloat(price) || 0;
    const d = parseFloat(discount) || 0;
    if (p <= 0) {
      setMsg('Price must be > 0');
      return;
    }
    if (d < 0 || d > 100) {
      setMsg('Discount 0-100');
      return;
    }
    const lineTotal = p * quantity * (1 - d / 100);
    const item: SalesItem = {
      id: genId(),
      barcode: padBarcode(barcode),
      price: p,
      discount: d,
      quantity,
      lineTotal: Math.round(lineTotal * 100) / 100,
    };
    setItems((prev) => [...prev, item]);
    setBarcode('');
    setPrice('');
    setDiscount('');
    setQuantity(1);
    setActiveField('barcode');
    setMsg('');
  };

  const removeItem = (id: string) => {
    setItems((prev) => prev.filter((i) => i.id !== id));
  };

  const grandTotal = items.reduce((s, i) => s + i.lineTotal, 0);

  const submit = () => {
    if (items.length === 0) {
      setMsg('Add items first');
      return;
    }
    const entry: SalesEntryType = {
      id: existingOpenEntry?.id || genId(),
      employeeId,
      employeeName: selectedEmployee?.name || 'Unknown',
      items,
      grandTotal: Math.round(grandTotal * 100) / 100,
      status: 'CLOSED',
      date: today,
      weekKey: getWeekKey(new Date()),
    };
    onSubmit(entry);
    setItems([]);
    setBarcode('');
    setPrice('');
    setDiscount('');
    setQuantity(1);
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
      <div className="flex-1 flex flex-col min-w-0 p-1.5 gap-1">
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
          {fieldBtn('discount', 'Disc%', discount ? `${discount}%` : '')}
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
        <div className="flex-1 overflow-y-auto min-h-0 border border-gray-800 rounded bg-gray-900/50">
          {items.length === 0 ? (
            <div className="text-gray-600 text-[9px] text-center py-2">No items added</div>
          ) : (
            <table className="w-full text-[8px]">
              <thead className="sticky top-0 bg-gray-900">
                <tr className="text-gray-500 uppercase">
                  <th className="px-0.5 text-left">Barcode</th>
                  <th className="px-0.5 text-right">Price</th>
                  <th className="px-0.5 text-right">D%</th>
                  <th className="px-0.5 text-right">Qty</th>
                  <th className="px-0.5 text-right">Total</th>
                  <th className="px-0.5 w-4"></th>
                </tr>
              </thead>
              <tbody>
                {items.map((item) => (
                  <tr key={item.id} className="border-t border-gray-800/50 text-green-400 font-mono">
                    <td className="px-0.5 truncate max-w-[70px]">{item.barcode}</td>
                    <td className="px-0.5 text-right">₱{item.price.toFixed(2)}</td>
                    <td className="px-0.5 text-right">{item.discount}%</td>
                    <td className="px-0.5 text-right">{item.quantity}</td>
                    <td className="px-0.5 text-right text-yellow-400">₱{item.lineTotal.toFixed(2)}</td>
                    <td className="px-0.5">
                      <button onClick={() => removeItem(item.id)} className="text-red-500 hover:text-red-300">
                        <Trash2 size={9} />
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* Grand total + Submit */}
        <div className="flex items-center gap-1">
          <div className="flex-1 text-[10px] text-gray-400">
            Total: <span className="text-yellow-400 font-bold text-sm">₱{grandTotal.toFixed(2)}</span>
            <span className="text-gray-600 ml-1">({items.length} items)</span>
          </div>
          <button
            onClick={submit}
            className="h-7 px-3 bg-cyan-700 hover:bg-cyan-600 rounded text-white text-[10px] font-bold flex items-center gap-1 active:scale-95 transition-transform"
          >
            <Send size={11} /> SUBMIT & CLOSE
          </button>
        </div>

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
