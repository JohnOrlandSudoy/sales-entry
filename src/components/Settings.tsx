import { useState } from 'react';
import type { AppSettings, Employee } from '../types';
import { genId } from '../storage';
import NumericKeypad from './NumericKeypad';
import { UserPlus, Trash2, Shield, KeyRound } from 'lucide-react';

interface Props {
  settings: AppSettings;
  onSave: (s: AppSettings) => void;
  onBack: () => void;
}

type PinField = 'system' | 'master' | null;

export default function Settings({ settings, onSave, onBack }: Props) {
  const [employees, setEmployees] = useState<Employee[]>(settings.employees);
  const [newName, setNewName] = useState('');
  const [systemPin, setSystemPin] = useState(settings.systemPin);
  const [masterPin, setMasterPin] = useState(settings.masterPin);
  const [editingPin, setEditingPin] = useState<PinField>(null);
  const [msg, setMsg] = useState('');

  const addEmployee = () => {
    const name = newName.trim();
    if (!name) return;
    if (employees.some((e) => e.name.toLowerCase() === name.toLowerCase())) {
      setMsg('Name exists');
      return;
    }
    setEmployees((prev) => [...prev, { id: genId(), name }]);
    setNewName('');
    setMsg('');
  };

  const removeEmployee = (id: string) => {
    setEmployees((prev) => prev.filter((e) => e.id !== id));
  };

  const save = () => {
    const newSettings: AppSettings = {
      systemPin: systemPin || settings.systemPin,
      masterPin: masterPin || settings.masterPin,
      employees,
    };
    onSave(newSettings);
    setMsg('Saved!');
    setTimeout(() => setMsg(''), 1500);
  };

  const pinVal = editingPin === 'system' ? systemPin : editingPin === 'master' ? masterPin : '';
  const pinSet = (v: string) => {
    if (v.length > 4) return;
    if (editingPin === 'system') setSystemPin(v);
    else if (editingPin === 'master') setMasterPin(v);
  };

  return (
    <div className="w-[480px] h-[288px] bg-gray-950 flex select-none overflow-hidden">
      {/* Left: Settings form */}
      <div className="flex-1 flex flex-col min-w-0 p-1.5 gap-1.5">
        <div className="flex items-center justify-between">
          <div className="text-[10px] text-yellow-400 font-bold uppercase tracking-wider flex items-center gap-1">
            <Shield size={11} /> Settings
          </div>
          <button onClick={onBack} className="text-[9px] text-gray-500 hover:text-gray-300">
            Back
          </button>
        </div>

        {/* Employee management */}
        <div className="flex-1 overflow-y-auto min-h-0">
          <div className="text-[9px] text-gray-500 uppercase tracking-wider mb-0.5">Employees</div>
          <div className="border border-gray-800 rounded bg-gray-900/50">
            {employees.map((emp) => (
              <div key={emp.id} className="flex items-center justify-between px-1.5 py-0.5 border-b border-gray-800/50 last:border-0">
                <span className="text-green-400 text-[10px]">{emp.name}</span>
                <button onClick={() => removeEmployee(emp.id)} className="text-red-500 hover:text-red-300">
                  <Trash2 size={10} />
                </button>
              </div>
            ))}
            {employees.length === 0 && (
              <div className="text-gray-600 text-[9px] text-center py-1">No employees</div>
            )}
          </div>
          <div className="flex gap-1 mt-1">
            <input
              value={newName}
              onChange={(e) => setNewName(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && addEmployee()}
              placeholder="New name..."
              className="flex-1 bg-gray-800 border border-gray-700 rounded px-1.5 py-0.5 text-[10px] text-green-400 outline-none focus:border-cyan-500"
            />
            <button
              onClick={addEmployee}
              className="h-6 px-2 bg-green-800 hover:bg-green-700 rounded text-white text-[9px] font-bold flex items-center gap-0.5 active:scale-95"
            >
              <UserPlus size={9} /> Add
            </button>
          </div>
        </div>

        {/* PIN settings */}
        <div className="border-t border-gray-800 pt-1">
          <div className="text-[9px] text-gray-500 uppercase tracking-wider mb-0.5">PIN Codes</div>
          <div className="flex gap-1">
            <button
              onClick={() => setEditingPin(editingPin === 'system' ? null : 'system')}
              className={`flex-1 h-6 rounded text-[9px] font-bold flex items-center justify-center gap-1 transition-colors ${
                editingPin === 'system'
                  ? 'bg-cyan-900/60 border border-cyan-500 text-cyan-400'
                  : 'bg-gray-800 border border-gray-700 text-gray-400 hover:border-gray-500'
              }`}
            >
              <KeyRound size={9} /> System: {systemPin}
            </button>
            <button
              onClick={() => setEditingPin(editingPin === 'master' ? null : 'master')}
              className={`flex-1 h-6 rounded text-[9px] font-bold flex items-center justify-center gap-1 transition-colors ${
                editingPin === 'master'
                  ? 'bg-cyan-900/60 border border-cyan-500 text-cyan-400'
                  : 'bg-gray-800 border border-gray-700 text-gray-400 hover:border-gray-500'
              }`}
            >
              <KeyRound size={9} /> Master: {masterPin}
            </button>
          </div>
        </div>

        {/* Save */}
        <button
          onClick={save}
          className="h-7 bg-cyan-700 hover:bg-cyan-600 rounded text-white text-xs font-bold active:scale-95 transition-transform"
        >
          SAVE SETTINGS
        </button>
        {msg && (
          <div className={`text-[9px] text-center font-bold ${msg === 'Saved!' ? 'text-green-400' : 'text-red-400'}`}>
            {msg}
          </div>
        )}
      </div>

      {/* Right: PIN keypad */}
      {editingPin && (
        <div className="w-[160px] p-1.5 border-l border-gray-800 bg-gray-900/30">
          <NumericKeypad
            value={pinVal}
            onChange={pinSet}
            placeholder={`${editingPin.toUpperCase()} PIN`}
            showDecimal={false}
            maxLen={4}
            onSubmit={() => setEditingPin(null)}
          />
        </div>
      )}
    </div>
  );
}
