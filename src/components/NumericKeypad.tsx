import { useState } from 'react';

interface Props {
  value: string;
  onChange: (v: string) => void;
  onSubmit?: () => void;
  placeholder?: string;
  showDecimal?: boolean;
  maxLen?: number;
  label?: string;
  dense?: boolean;
}

export default function NumericKeypad({
  value,
  onChange,
  onSubmit,
  placeholder = '',
  showDecimal = true,
  maxLen = 15,
  label,
  dense = false,
}: Props) {
  const [active, setActive] = useState<string | null>(null);

  const press = (key: string) => {
    setActive(key);
    setTimeout(() => setActive(null), 100);
    if (key === 'back') {
      onChange(value.slice(0, -1));
      return;
    }
    if (key === '.') {
      if (value.includes('.')) return;
      onChange(value + '.');
      return;
    }
    if (value.length >= maxLen) return;
    onChange(value + key);
  };

  const keys = showDecimal
    ? [
        ['1', '2', '3'],
        ['4', '5', '6'],
        ['7', '8', '9'],
        ['.', '0', 'back'],
      ]
    : [
        ['1', '2', '3'],
        ['4', '5', '6'],
        ['7', '8', '9'],
        ['', '0', 'back'],
      ];

  return (
    <div className="flex flex-col gap-1 w-full">
      {label && (
        <div className="text-[9px] text-yellow-500 mb-0.5 font-bold uppercase tracking-widest font-mono">{label}</div>
      )}
      <div
        className={`bg-black border border-gray-800 rounded-lg px-2 text-right font-mono text-green-500 overflow-hidden whitespace-nowrap shadow-inner ${
          dense ? 'py-1 text-lg min-h-[30px]' : 'py-1.5 text-xl min-h-[36px]'
        }`}
      >
        {value || <span className="text-gray-700 text-sm">{placeholder}</span>}
      </div>
      <div className={`grid grid-cols-3 ${dense ? 'gap-0.5 mt-0.5' : 'gap-1 mt-1'}`}>
        {keys.map((row) =>
          row.map((key) => {
            const isBack = key === 'back';
            const isDot = key === '.';
            const display = isBack ? '\u232B' : key;
            const isActive = active === key;
            
            let btnClass = "bg-gray-900 text-green-500 border border-gray-800";
            if (isBack) btnClass = "bg-red-950/40 text-red-500 border border-red-900/50";
            if (isDot) btnClass = "bg-gray-800 text-yellow-500 border border-gray-700";

            return (
              <button
                key={key}
                onClick={() => press(key)}
                className={`
                  ${dense ? 'h-[32px] text-base rounded-md' : 'h-[40px] text-lg rounded-lg'} font-bold transition-all duration-75 flex items-center justify-center
                  ${btnClass}
                  ${isActive ? 'scale-90 bg-gray-800 brightness-125' : 'hover:bg-gray-800'}
                  active:scale-95
                `}
              >
                {display}
              </button>
            );
          })
        )}
      </div>
      {onSubmit && (
        <button
          onClick={onSubmit}
          className={`bg-green-900/40 border border-green-800 text-green-400 text-xs font-bold uppercase tracking-widest active:scale-95 transition-all hover:bg-green-800/40 ${
            dense ? 'mt-0.5 h-[34px] rounded-md' : 'mt-1 h-[40px] rounded-lg'
          }`}
        >
          ENTER
        </button>
      )}
    </div>
  );
}
