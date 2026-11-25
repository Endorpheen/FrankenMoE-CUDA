"""Deterministic hard-question fixtures with independent expected outputs."""

ledger = [
    ('A',+18000,'ok'),('B',-24500,'ok'),('C',-44800,'hold'),('D',+9900,'ok'),
    ('E',-5999,'ok'),('F',+7200,'void'),('G',-22000,'ok'),('H',-40100,'ok'),
    ('I',+13200,'ok'),('J',-14800,'hold'),('K',+3800,'ok'),('L',-60000,'ok'),('M',-250,'ok'),
]
balance=125000
rejected=[]
for code,change,status in ledger:
    if status!='ok':
        continue
    if balance+change<20000:
        rejected.append(code)
    else:
        balance+=change
ledger_prompt="""An account opens at 125000 cents. Process the following independent events strictly in listed order. Each value is an integer cent amount. An event marked hold or void is skipped, with no impact on the balance or the rejected list. Apply all positive ok events. Apply a negative ok event only if the resulting balance is at least 20000 cents. If a negative ok event would violate that floor, leave the balance unchanged and append its ID to rejected. A rejection does not block subsequent events. Do not round amounts, sort the events or update the minimum. Treat the strings ok, hold and void as literal flags, with no additional accounting rules.
"""
ledger_prompt+='\n'.join(f'{k}: {v:+d} {flag}' for k,v,flag in ledger)
ledger_prompt+="""\nReturn only a JSON object with keys balance and rejected. The latter is an array of event IDs in arrival order. Verify the final balance after the last event, including any recent credit."""

jobs=[
    ('R1',3,8,6),('R2',9,12,5),('R3',7,10,7),('R4',11,18,4),
    ('R5',-2,4,2),('R6',18,22,3),('R7',4,4,8),('R8',20,23,4),
    ('R9',12,14,1),('R10',0,3,9),('R11',9,11,6),('R12',22,24,10),
]
used=[]
accepted=[]
for name,a,b,priority in sorted(jobs,key=lambda r:(-r[3],int(r[0][1:]))):
    if not 0<=a<b<=24:
        continue
    if any(max(a,x)<min(b,y) for x,y in used):
        continue
    used.append((a,b))
    accepted.append(name)
ru="""Реши задачу планирования без внешних данных. Обслуживание возможно в полуоткрытых интервалах [начало, конец) внутри [0,24). Нулевой или отрицательный интервал, отрицательное начало и конец больше 24 недопустимы. Сначала исключи недопустимые заявки. Затем рассматривай заявки по убыванию приоритета, при равном приоритете — по возрастанию номера после буквы R. Добавляй заявку только если она не пересекается с уже выбранными. Соприкосновение границ не считается пересечением. Выбранные заявки уже нельзя заменить, даже если другой набор даёт больше суммарного времени. Ответ — только JSON с ключом accepted, массив id строго в порядке принятия. Таблица ниже содержит id, начало, конец, приоритет.
"""
ru+='\n'.join(f'{k} {a} {b} {v}' for k,a,b,v in jobs)

steps=[
    [1,4,7,-1,3],[5,3,11,2],[2,-4,0,8,8,1],[9,1,2,3,4],
    [0,0,1,1,2,2],[7,6,5,4,3,2,1],[11,0,11,-3,2],[3,6,9,12],
    [-7,5,10,-1,5],[12,12,7,6,5],[22,3,4,5,10,7],[8,9,2,17,4],
]
def step_trace(xs):
    state=0
    out=[]
    for index,value in enumerate(xs):
        if value<0:
            state=state//2
        elif index%2==0:
            state=(state+value)%17
        else:
            state=(2*state+value)%17
        out.append(state)
    return out

code_prompt="""Audit a state machine. Each row is an independent call to trace(xs), with zero-based indexing and fresh state. The complete program is below. Output only a JSON object with key traces, a list of all twelve arrays in row order. Each array must include every intermediate state, in the order appended by the function. Reproduce the integer arithmetic as written, including the priority of the negative branch. There is no shared state across rows and no omitted initialization.

def trace(xs):
    state = 0
    out = []
    for index, value in enumerate(xs):
        if value < 0:
            state = state // 2
        elif index % 2 == 0:
            state = (state + value) % 17
        else:
            state = (2 * state + value) % 17
        out.append(state)
    return out

Rows:
"""
code_prompt+='\n'.join(f'{i}: {r}' for i,r in enumerate(steps,1))
CASES=[
    dict(id='ledger',prompt=ledger_prompt,expected=dict(balance=balance,rejected=rejected),budget=128),
    dict(id='scheduler-ru',prompt=ru,expected=dict(accepted=accepted),budget=128),
    dict(id='code-trace',prompt=code_prompt,expected=dict(traces=[step_trace(x) for x in steps]),budget=320),
]

incidents=[
    ('INC-41','prod','database','closed'),('INC-42','prod','database','open'),
    ('INC-43','stage','database','open'),('INC-44','prod','search','open'),
    ('INC-45','prod','database','open'),('INC-46','prod','database','closed'),
    ('INC-47','dev','database','open'),('INC-48','prod','cache','open'),
    ('INC-49','prod','database','open'),('INC-50','prod','database','closed'),
    ('INC-51','stage','search','open'),('INC-52','prod','database','open'),
]
extract="""A release lead is preparing a machine-readable handoff. Use only the incident list below. Extract every incident whose environment is exactly prod, component exactly database, and status exactly open. Apply all three matching criteria together. Entries marked closed are excluded even if their environment and component match. Entries from dev or stage are excluded. Preserve the input order and preserve the incident identifier exactly as written. Do not list any other components or invent missing incidents. Return a JSON object with exactly the key open_database_prod, whose value is an array of matching identifiers. The columns below are ID, environment, component and status.
"""
extract+='\n'.join(' '.join(row) for row in incidents)
CASES.append(dict(id='incident-extraction',prompt=extract,budget=128,
    expected=dict(open_database_prod=[row[0] for row in incidents if row[1:]==('prod','database','open')])))
