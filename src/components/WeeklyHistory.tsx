import { useState } from 'react';
import type { SalesEntry, WeekRecord } from '../types';
import { buildWeekRecords } from '../storage';
import { Calendar, Trash2, ChevronDown, ChevronRight } from 'lucide-react';

interface Props {
  entries: SalesEntry[];
  onClearWeek: (weekKey: string) => void;
  masterPin: string;
  onBack: () => void;
}

export default function WeeklyHistory({ entries, onClearWeek, masterPin, onBack }: Props) {
  const [expandedWeek, setExpandedWeek] = useState<string | null>(null);
  const [clearConfirm, setClearConfirm] = useState<string | null>(null);
  const [pinInput, setPinInput] = useState('');
  const [pinError, setPinError] = useState(false);

  const weekRecords: WeekRecord[] = buildWeekRecords(entries);

  const toggleWeek = (weekKey: string) => {
    setExpandedWeek(expandedWeek === weekKey ? null : weekKey);
  };

  const requestClear = (weekKey: string) => {
    setClearConfirm(weekKey);
    setPinInput('');
    setPinError(false);
  };

  const isMonday = new Date().getDay() === 1;

  return (
    <div className="w-[480px] h-[288px] bg-gray-950 flex flex-col select-none overflow-hidden">
      {/* Monday notification */}
      {isMonday && (
        <div className="bg-orange-900/40 border-b border-orange-800 px-2 py-1 flex items-center justify-between">
          <span className="text-orange-400 text-[9px] font-bold">Monday detected - Consider clearing last week's history</span>
          {weekRecords.length > 1 && (
            <button
              onClick={() => requestClear(weekRecords[1]?.weekKey || '')}
              className="text-[8px] bg-orange-800 hover:bg-orange-700 px-1.5 py-0.5 rounded text-white font-bold"
            >
              Clear Last Week
            </button>
          )}
        </div>
      )}

      <div className="flex items-center justify-between p-1.5 pb-0">
        <div className="text-[10px] text-yellow-400 font-bold uppercase tracking-wider flex items-center gap-1">
          <Calendar size={11} /> Weekly History
        </div>
        <button onClick={onBack} className="text-[9px] text-gray-500 hover:text-gray-300">
          Back
        </button>
      </div>

      {/* Week list */}
      <div className="flex-1 overflow-y-auto min-h-0 p-1.5 pt-1">
        {weekRecords.length === 0 ? (
          <div className="text-gray-600 text-[10px] text-center py-4">No sales history</div>
        ) : (
          weekRecords.map((wr) => {
            const isExpanded = expandedWeek === wr.weekKey;
            const weekTotal = wr.entries.reduce((s, e) => s + e.grandTotal, 0);
            return (
              <div key={wr.weekKey} className="mb-1 border border-gray-800 rounded bg-gray-900/50">
                <button
                  onClick={() => toggleWeek(wr.weekKey)}
                  className="w-full flex items-center justify-between px-1.5 py-1 hover:bg-gray-800/50"
                >
                  <div className="flex items-center gap-1">
                    {isExpanded ? <ChevronDown size={10} className="text-gray-500" /> : <ChevronRight size={10} className="text-gray-500" />}
                    <span className="text-[10px] text-cyan-400 font-semibold">{wr.weekLabel}</span>
                  </div>
                  <div className="flex items-center gap-2">
                    <span className="text-[9px] text-yellow-400 font-mono">₱{weekTotal.toFixed(2)}</span>
                    <span className="text-[8px] text-gray-600">{wr.entries.length} entries</span>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        requestClear(wr.weekKey);
                      }}
                      className="text-red-500 hover:text-red-300"
                    >
                      <Trash2 size={9} />
                    </button>
                  </div>
                </button>
                {isExpanded && (
                  <div className="border-t border-gray-800/50 px-1.5 py-0.5">
                    {wr.entries.map((entry) => (
                      <div key={entry.id} className="flex items-center justify-between py-0.5 border-b border-gray-800/30 last:border-0">
                        <div className="text-[9px] flex flex-col">
                          <div>
                            <span className="text-green-400 font-bold">{entry.employeeName}</span>
                            <span className="text-gray-600 ml-1">({entry.items.length} items)</span>
                          </div>
                          <div className="flex flex-wrap gap-x-2 gap-y-0.5 mt-0.5">
                            {entry.items.map((item, idx) => (
                              <span key={idx} className="text-[7px] text-cyan-500 font-mono">
                                {item.barcode}
                              </span>
                            ))}
                          </div>
                        </div>
                        <div className="flex items-center gap-1">
                          <span className="text-yellow-400 text-[9px] font-mono">₱{entry.grandTotal.toFixed(2)}</span>
                          <span
                            className={`text-[7px] px-1 py-0.5 rounded font-bold ${
                              entry.status === 'CLOSED'
                                ? 'bg-green-900/60 text-green-400'
                                : 'bg-orange-900/60 text-orange-400'
                            }`}
                          >
                            {entry.status}
                          </span>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>
            );
          })
        )}
      </div>

      {/* Clear confirmation modal */}
      {clearConfirm && (
        <div className="absolute inset-0 bg-black/80 flex items-center justify-center z-50">
          <div className="bg-gray-900 border border-gray-700 rounded p-3 w-[240px]">
            <div className="text-yellow-400 text-xs font-bold mb-2">Master PIN Required</div>
            <div className="text-gray-400 text-[9px] mb-2">
              Enter Master PIN to clear week of {clearConfirm}
            </div>
            <div className="flex gap-1 mb-2">
              {[0, 1, 2, 3].map((i) => (
                <div
                  key={i}
                  className={`w-6 h-6 rounded-full border-2 flex items-center justify-center ${
                    i < pinInput.length
                      ? pinError
                        ? 'border-red-500 bg-red-500'
                        : 'border-cyan-400 bg-cyan-400'
                      : 'border-gray-600'
                  }`}
                >
                  {i < pinInput.length && <div className={`w-2 h-2 rounded-full ${pinError ? 'bg-red-300' : 'bg-cyan-200'}`} />}
                </div>
              ))}
            </div>
            {pinError && <div className="text-red-400 text-[9px] mb-1 animate-pulse">WRONG PIN</div>}
            <div className="grid grid-cols-3 gap-1">
              {['1','2','3','4','5','6','7','8','9','C','0','\u232B'].map((key) => {
                const isBack = key === '\u232B';
                const isClear = key === 'C';
                return (
                  <button
                    key={key}
                    onClick={() => {
                      if (isClear) { setPinInput(''); setPinError(false); return; }
                      if (isBack) { setPinInput((p) => p.slice(0, -1)); setPinError(false); return; }
                      if (pinInput.length >= 4) return;
                      const next = pinInput + key;
                      setPinInput(next);
                      if (next.length === 4) {
                        setTimeout(() => {
                          if (next === masterPin) {
                            onClearWeek(clearConfirm);
                            setClearConfirm(null);
                            setPinInput('');
                          } else {
                            setPinError(true);
                            setTimeout(() => { setPinError(false); setPinInput(''); }, 800);
                          }
                        }, 150);
                      }
                    }}
                    className={`h-8 rounded text-sm font-bold ${
                      isBack ? 'bg-red-900/80 text-red-300' : isClear ? 'bg-orange-900/80 text-orange-300' : 'bg-gray-800 text-green-400'
                    } hover:opacity-80 active:scale-95`}
                  >
                    {key}
                  </button>
                );
              })}
            </div>
            <button
              onClick={() => { setClearConfirm(null); setPinInput(''); }}
              className="mt-1.5 w-full h-6 bg-gray-800 rounded text-gray-400 text-[9px] hover:bg-gray-700"
            >
              Cancel
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
